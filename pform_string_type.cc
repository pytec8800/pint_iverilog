/*
 * Copyright (c) 2012-2014 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

# include  "pform.h"
# include  "parse_misc.h"
# include  "ivl_assert.h"

static void pform_set_string_type(const struct vlltype&li, const string_type_t*, perm_string name, NetNet::Type net_type, list<named_pexpr_t>*attr)
{
      PWire*net = pform_get_make_wire_in_scope(li, name, net_type, NetNet::NOT_A_PORT, IVL_VT_STRING);
      assert(net);
      pform_bind_attributes(net->attributes, attr, true);
}

void pform_set_string_type(const struct vlltype&li, const string_type_t*string_type, list<perm_string>*names, NetNet::Type net_type, list<named_pexpr_t>*attr)
{
      for (list<perm_string>::iterator cur = names->begin()
		 ; cur != names->end() ; ++ cur) {
	    pform_set_string_type(li, string_type, *cur, net_type, attr);
      }
}
