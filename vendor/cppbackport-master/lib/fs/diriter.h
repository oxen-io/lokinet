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
#ifndef PBL_CPP_FS_DIRITER_H
#define PBL_CPP_FS_DIRITER_H

#include <stack>
#include "direntry.h"

namespace cpp17
{
namespace filesystem
{

/** An iterator for traversing directory contents
 *
 * This class can be used for iterating over the contents of a directory. The
 * parameterized constructor will make a "begin" iterator, and the default
 * constructor will make an "end" iterator. One can dereference an iterator
 * to get a directory_entry describing the file/subdirectory.
 *
 * @note The "." and ".." 'files' are ignored by this iterator, since they are
 * not 'in' the directory.
 *
 * @note These objects are not copyable
 *
 * @note Only end iterators are ever considered equal. In particular, two
 * directory_iterator-s constructed with the same argument may not point to the
 * same file.
 */
class directory_iterator
{
public:
	/** Construct an iterator for a directory
	 *
	 * After construction, the iterator will point to the first file in the
	 * directory.
	 */
	explicit directory_iterator(const path& path_);

	/** Construct an end iterator
	 */
	directory_iterator();

	/** Destructor
	 */
	~directory_iterator();
    
    directory_iterator begin();
    directory_iterator end();

	/** Test if two iterators are equal
	 *
	 * Only two end iterators are equal
	 */
	bool operator==(const directory_iterator& i) const;

	/** Test if two iterators are not equal
	 *
	 * Only two end iterators are equal
	 */
	bool operator!=(const directory_iterator& i) const;

	/** Move to the next file/subdirectory
	 */
	directory_iterator& operator++();

	/** Get information for the file system object
	 *
	 * @note Causes a stat. If all you need is the type of the file system object,
	 * use the cheaper type() member
	 */
	const directory_entry& operator*() const;

	/** Get information for the file system object
	 *
	 * @note Causes a stat. If all you need is the type of the file system object,
	 * use the cheaper type() member
	 */
	const directory_entry* operator->() const;

	file_type type() const;

	void swap(directory_iterator&);

    // non-copyable
    // now it is
    directory_iterator(const directory_iterator&);
    directory_iterator& operator=(const directory_iterator&);
    
    directory_iterator(const directory_iterator&, int pos);
    
    //void *endPtr;
private:
	class impl;


	impl* pimpl;
};

class recursive_directory_iterator
{
public:
	recursive_directory_iterator();
	explicit recursive_directory_iterator(const path& p);
	~recursive_directory_iterator();

	recursive_directory_iterator& operator++();
	const directory_entry& operator*() const;
	const directory_entry* operator->() const;
	bool operator==(const recursive_directory_iterator& i) const;
	bool operator!=(const recursive_directory_iterator& i) const;
private:
	// non-copyable
	recursive_directory_iterator(const recursive_directory_iterator&);
	recursive_directory_iterator& operator=(const recursive_directory_iterator&);

	bool descend(const path& p);
	void ascend();

	std::stack< directory_iterator* > stack;
};

}
}
#endif // PBL_FS_DIRITER_H
