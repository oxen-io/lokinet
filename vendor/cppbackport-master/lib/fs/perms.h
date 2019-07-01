/* Copyright (c) 2015, Pollard Banknote Limited
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef PBL_CPP_FS_PERMS_H
#define PBL_CPP_FS_PERMS_H

#include <iosfwd>

namespace perms
{
/** File permissions
 *
 * See: std::experimental::filesystem::perms
 */
enum perms
{
	none             = 0,
	owner_read       = 0400, // S_IRUSR
	owner_write      = 0200, // S_IWUSR
	owner_exec       = 0100, // S_IXUSR
	owner_all        = 0700, // S_IRWXU
	group_read       = 040,  // S_IRGRP
	group_write      = 020,  // S_IWGRP
	group_exec       = 010,  // S_IXGRP
	group_all        = 070,  // S_IRWXG
	others_read      = 04,   // S_IROTH
	others_write     = 02,   // S_IWOTH
	others_exec      = 01,   // S_IXOTH
	others_all       = 07,   // S_IRWXO
	all              = 0777,
	set_uid          = 04000,
	set_gid          = 02000,
	sticky_bit       = 01000,
	mask             = 07777,
	unknown          = 0xffff,
	add_perms        = 0x10000,
	remove_perms     = 0x20000,
	resolve_symlinks = 0x40000
};

inline perms operator|(
	perms a,
	perms b
)
{
	return static_cast< perms >( static_cast< int >( a ) | static_cast< int >( b ) );
}

inline perms operator&(
	perms a,
	perms b
)
{
	return static_cast< perms >( static_cast< int >( a ) & static_cast< int >( b ) );
}

inline perms operator^(
	perms a,
	perms b
)
{
	return static_cast< perms >( static_cast< int >( a ) ^ static_cast< int >( b ) );
}

std::ostream& operator<<(std::ostream&, perms);

}

namespace cpp17
{
namespace filesystem
{
typedef ::perms::perms perms;
}
}
#endif // PBL_CPP_FS_PERMS_H
