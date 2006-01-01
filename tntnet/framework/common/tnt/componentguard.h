/* tnt/componentpointer.h
   Copyright (C) 2003-2005 Tommi Maekitalo

This file is part of tntnet.

Tntnet is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

Tntnet is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with tntnet; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330,
Boston, MA  02111-1307  USA
*/

#ifndef TNT_COMPONENTGUARD_H
#define TNT_COMPONENTGUARD_H

namespace tnt
{
  class ComponentGuard
  {
      Component* comp;

      // non compyable
      ComponentGuard(const ComponentGuard&);
      ComponentGuard& operator=(const ComponentGuard&);

    public:
      ComponentGuard(Component* comp_)
        : comp(comp_)
        { }
      ~ComponentGuard()
        { comp->drop(); }

      const Component* getPtr() const      { return comp; }
      Component* getPtr()                  { return comp; }
      operator const Component* () const   { return comp; }
      const Component* operator-> () const { return comp; }
      Component* operator-> ()             { return comp; }
  };

}

#endif // TNT_COMPONENTGUARD_H