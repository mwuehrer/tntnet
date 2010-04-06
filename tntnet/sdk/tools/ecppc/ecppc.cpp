/*
 * Copyright (C) 2003-2005 Tommi Maekitalo
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * As a special exception, you may use this file as part of a free
 * software library without restriction. Specifically, if other files
 * instantiate templates or use macros or inline functions from this
 * file, or you compile this file and link it with other files to
 * produce an executable, this file does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License. This exception does not however invalidate any other
 * reasons why the executable file might be covered by the GNU Library
 * General Public License.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "tnt/ecppc/ecppc.h"
#include "tnt/ecppc/generator.h"
#include "tnt/ecppc/dependencygenerator.h"
#include <tnt/mimedb.h>
#include <fstream>
#include <sstream>
#include <functional>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "config.h"
#include <cxxtools/loginit.h>
#include <cxxtools/log.h>

log_define("tntnet.ecppc")

namespace tnt
{
  namespace ecppc
  {
    Ecppc::Ecppc(int& argc, char* argv[])
      : requestname(cxxtools::Argp<std::string>(argc, argv, 'n')),
#ifdef HAVE_CXXTOOLS_ARGP
        ofile(cxxtools::Argp<std::string>(argc, argv, 'o')),
        odir(cxxtools::Argp<std::string>(argc, argv, 'O')),
#else
        ofile(cxxtools::Arg<std::string>(argc, argv, 'o')),
        odir(cxxtools::Arg<std::string>(argc, argv, 'O')),
#endif
        mimetype(argc, argv, 'm'),
        mimedb(argc, argv, "--mimetypes", "/etc/mime.types"),
        binary(argc, argv, 'b'),
        multibinary(argc, argv, 'b'),
        keepPath(argc, argv, 'p'),
        componentclass(argc, argv, 'C'),
        compress(argc, argv, 'z'),
        verbose(argc, argv, 'v'),
        debug(argc, argv, 'd'),
        generateDependencies(argc, argv, 'M'),
        disableLinenumbers(argc, argv, 'L')
    {
      while (true)
      {
#ifdef HAVE_CXXTOOLS_ARGP
        cxxtools::Argp<const char*> include(argc, argv, 'I', 0);
#else
        cxxtools::Arg<const char*> include(argc, argv, 'I', 0);
#endif
        if (!include.isSet())
          break;
        includes.push_back(include.getValue());
      }

      if (argc < 2 || argv[1][0] == '-')
        throw Usage(argv[0]);
      inputfile = argv[1];
      if (multibinary)
      {
        for (int a = 1; a < argc; ++a)
        {
          std::string inputfile = argv[a];
          std::string key = inputfile;
          if (!keepPath)
          {
            // strip path
            std::string::size_type p;
            if ((p = key.find_last_of('/')) != std::string::npos)
              key.erase(0, p + 1);
          }

          inputfiles.insert(inputfiles_type::value_type(key, inputfile));
        }
      }

      if (debug)
        log_init_debug();
      else
        log_init();
    }

    int Ecppc::run()
    {
      // requestname from inputfilename
      if (requestname.empty())
      {
        if (multibinary)
        {
          std::cerr << "warning: no requestname passed (with -n) - using \"images\"" << std::endl;
          requestname = "images";
        }
        else
        {
          std::string input = inputfile;
          std::string::size_type pos_dot = input.find_last_of(".");
          if (pos_dot != std::string::npos)
          {
            std::string::size_type pos_slash;
            if (keepPath || (pos_slash = input.find_last_of("\\/")) == std::string::npos)
              requestname = input.substr(0, pos_dot);
            else if (pos_slash < pos_dot)
              requestname = input.substr(pos_slash + 1, pos_dot - pos_slash - 1);

            if (ofile.empty() && !generateDependencies)
              ofile = input.substr(0, pos_dot);

            extname = input.substr(pos_dot + 1);
          }
          else
          {
            requestname = input;
          }

          if (requestname.empty())
          {
            std::cerr << "cannot derive component name from filename. Use -n" << std::endl;
            return -1;
          }
        }
      }

      if (ofile.empty() && !generateDependencies)
        ofile = requestname;

      if (generateDependencies)
        return runDependencies();
      else
        return runGenerator();
    }

    int Ecppc::runGenerator()
    {
      // strip cpp-extension from outputfilename
      if (ofile.size() == ofile.rfind(".cpp") + 4)
        ofile = ofile.substr(0, ofile.size() - 4);

      // create generator
      tnt::ecppc::Generator generator(requestname);

      // initialize
      generator.enableLinenumbers(!disableLinenumbers);
      Bodypart::enableLinenumbers(!disableLinenumbers);

      if (mimetype.isSet())
        generator.setMimetype(mimetype);
      else if (!extname.empty() && !multibinary)
      {
        tnt::MimeDb db(mimedb);
        std::string mimeType = db.getMimetype(extname);
        if (mimeType.empty())
        {
          if (extname != "ecpp")
            std::cerr << "warning: unknown mimetype" << std::endl;
        }
        else
          generator.setMimetype(mimeType);
      }

      generator.setCompress(compress);

      if (componentclass.isSet())
        generator.setComponentclass(componentclass);

      std::string obase = odir;
      if (!obase.empty())
        obase += '/';
      obase += ofile;
      
      //
      // parse sourcefile
      //

      if (multibinary)
      {
        tnt::MimeDb mimeDb;
        if (!mimetype.isSet())
          mimeDb.read(mimedb);

        for (inputfiles_type::const_iterator it = inputfiles.begin();
             it != inputfiles.end(); ++it)
        {
          std::string key = it->first;
          std::string inputfile = it->second;

          struct stat st;
          if (stat(inputfile.c_str(), &st) != 0)
          {
            // search for input file in includes list
            int ret;
            for (includes_type::const_iterator incl = includes.begin();
              incl != includes.end(); ++incl)
            {
              inputfile = *incl + '/' + it->second;
              if ((ret = stat(inputfile.c_str(), &st)) != 0)
                break;
            }

            if (ret != 0)
              throw std::runtime_error("can't stat " + it->second);
          }

          std::ifstream in(inputfile.c_str());
          if (!in)
            throw std::runtime_error("can't read " + inputfile);

          std::ostringstream content;
          content << in.rdbuf();

          std::string mime;
          if (mimetype.isSet())
            mime = mimetype;
          else
          {
            mime = mimeDb.getMimetype(inputfile);
            if (mime.empty())
            {
              mime = "application/x-data";
              std::cerr << "warning: no mimetype found for \"" << inputfile << "\" using " << mime << std::endl;
            }
          }

          generator.addImage(key, content.str(), mime, st.st_ctime);
        }
      }
      else
      {
        std::ifstream in(inputfile);
        if (!in)
          throw std::runtime_error(std::string("can't read ") + inputfile);

        if (binary)
        {
          std::ostringstream html;
          html << in.rdbuf();
          generator.onHtml(html.str());
          generator.setRawMode();

          struct stat st;
          if (stat(inputfile, &st) == 0)
            generator.setLastModifiedTime(st.st_ctime);
        }
        else
          runParser(in, generator);
      }

      //
      // generate Code
      //
      if (verbose)
        std::cout << "generate " << obase << ".cpp" << std::endl;
      std::ofstream sout((obase + ".cpp").c_str());
      generator.getCppWoHeader(sout, ofile + ".cpp");
      sout.close();

      if (!sout)
        throw std::runtime_error("error writing file \"" + ofile + ".cpp\"");

      return 0;
    }

    int Ecppc::runDependencies()
    {
      log_trace("runDependencies");

      tnt::ecppc::Dependencygenerator generator(requestname, inputfile);

      std::ifstream in(inputfile);
      if (!in)
        throw std::runtime_error(std::string("can't read ") + inputfile);

      if (!binary)
        runParser(in, generator);

      if (ofile.empty())
        generator.getDependencies(std::cout);
      else
      {
        std::ofstream out(ofile.c_str());
        generator.getDependencies(out);
      }

      return 0;
    }

    void Ecppc::runParser(std::istream& in, tnt::ecpp::ParseHandler& handler)
    {
      // create parser
      tnt::ecpp::Parser parser(handler, inputfile);

      // initialize parser
      for (includes_type::const_iterator it = includes.begin();
           it != includes.end(); ++it)
        parser.addInclude(*it);

      // call parser
      parser.parse(in);
    }

    Usage::Usage(const char* progname)
    {
      std::ostringstream o;
      o << PACKAGE_STRING "\n\n"
           "ecppc-compiler\n\n"
           "usage: " << progname << " [options] ecpp-source\n\n"
           "  -o filename      outputfile\n"
           "  -n name          componentname\n"
           "  -I dir           include-directory\n"
           "  -m type          Mimetype\n"
           "  --mimetypes file read mimetypes from file (default /etc/mime.types)\n"
           "  -b               binary\n"
           "  -bb              generate multibinary component\n"
           "  -z               compress constant data\n"
           "  -v               verbose\n"
           "  -M               generate dependency for Makefile\n"
           "  -p               keep path when generating component name from filename\n"
           "  -L               disable generation of #line-directives\n";
      msg = o.str();
    }
  }
}

int main(int argc, char* argv[])
{
  std::ios::sync_with_stdio(false);

  try
  {
    tnt::ecppc::Ecppc app(argc, argv);
    return app.run();
  }
  catch(const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
