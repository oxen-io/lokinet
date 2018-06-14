/* Copyright (c) 2014, Pollard Banknote Limited
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
#include "copyfile.h"

#include <cerrno>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace cpp17
{
namespace filesystem
{

/** Try to do copy the file as safely as possible (esp., gracefully handle
 * errors, avoid race conditions).
 *
 * @bug If there's an error while overwriting a file, the original is lost
 */
bool copy_file(
	const path&  source,
	const path&  dest,
	copy_options opt
)
{
	const int in = ::open(source.c_str(), O_RDONLY | O_CLOEXEC);

	if ( in != -1 )
	{
		struct stat instat;

		if ( ::fstat(in, &instat) == 0 && ( S_ISREG(instat.st_mode) || S_ISLNK(instat.st_mode) ) )
		{
			// Get the destination file
			int out = ::open(dest.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, S_IWUSR);

			if ( out == -1 && errno == EEXIST )
			{
				// File already exists -- maybe we will overwrite it
				out = ::open(dest.c_str(), O_WRONLY | O_CLOEXEC);

				if ( out != -1 )
				{
					bool err = false;

					struct stat outstat;

					if ( ( ::fstat(out, &outstat) != 0 ) || ( instat.st_dev == outstat.st_dev && instat.st_ino == outstat.st_ino ) || ( ( opt & 7 ) == 0 ) )
					{
						// Couldn't stat, Same file, or bad copy_options
						err = true;
					}
					else
					{
						// Replace file or not, depending on flags
						if ( (opt& copy_options::overwrite_existing) || ( (opt& copy_options::update_existing) && ( instat.st_mtime > outstat.st_mtime ) ) )
						{
							// replace the existing file
							if ( ::ftruncate(out, 0) != 0 )
							{
								err = true;
							}
						}
						else
						{
							// do nothing
							::close(out);
							::close(in);

							return false;
						}
					}

					if ( err )
					{
						::close(out);
						out = -1;
					}
				}
			}

			// Do we have a destination file?
			if ( out != -1 )
			{
				bool err = false;

				while ( !err )
				{
					char buf[4096];

					const ssize_t n = ::read( in, buf, sizeof( buf ) );

					if ( n == -1 )
					{
						err = true;
					}
					else if ( n == 0 )
					{
						// EOF - copy has been successful
						::fchmod(out, instat.st_mode & 0777);
						::close(out);
						::close(in);

						return true;
					}
					else
					{
						const char* p = buf;

						while ( p - buf < n )
						{
							const ssize_t m = ::write( out, p, static_cast< std::size_t >( n - ( p - buf ) ) );

							if ( m == -1 )
							{
								err = true;
								break;
							}

							p += m;
						}
					}
				}

				// Remove the incomplete file
				::unlink( dest.c_str() );
				::close(out);
			}
		}

		::close(in);
	}

	return false;
}

bool copy_file(
	const path& source,
	const path& dest
)
{
	return copy_file(source, dest, ::copy_options::none);
}

}
}
