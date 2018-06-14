/* Copyright (c) 2016, Pollard Banknote Limited
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
#ifndef PBL_CPP_UNORDERED_SET_H
#define PBL_CPP_UNORDERED_SET_H

#include "version.h"

#ifdef CPP11
#include <unordered_set>
#else
#include <vector>
#include <algorithm>
#include "memory.h"
#include "functional.h"

namespace cpp11
{
template< class Key, class Hash = hash< Key >, class KeyEqual = std::equal_to< Key > >
class unordered_set
{
	struct node
	{
		node(
			node*       p,
			std::size_t h,
			const Key&  x
		)
			: next(p), hash(h), value(x)
		{

		}

		node* next;
		std::size_t hash;
		Key value;
	};
public:
	typedef Key key_type;
	typedef Key value_type;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;
	typedef Hash hasher;
	typedef KeyEqual key_equal;
	typedef value_type& reference;
	typedef const value_type& const_reference;
	typedef Key* pointer;
	typedef const Key* const_pointer;

	class const_iterator
	{
public:
		const_iterator()
			: curr(0), bin(0), parent(0)
		{
		}

		const_iterator& operator++()
		{
			if ( curr )
			{
				if ( curr->next )
				{
					// next within same bin
					curr = curr->next;
				}
				else
				{
					// next bin
					const std::size_t n = parent->bins.size();

					for ( std::size_t i = bin + 1; i < n; ++i )
					{
						if ( parent->bins[i] )
						{
							curr = parent->bins[i];
							bin  = i;
							return *this;
						}
					}

					curr = 0;
					bin  = n;
				}
			}

			return *this;
		}

		const_reference operator*() const
		{
			return curr->value;
		}

		bool operator==(const const_iterator& o) const
		{
			return parent == o.parent && curr == o.curr;
		}

		bool operator!=(const const_iterator& o) const
		{
			return parent != o.parent || curr != o.curr;
		}

private:
		friend class unordered_set;

		const_iterator(
			const unordered_set* p,
			node*                n,
			std::size_t          b
		)
			: parent(p), curr(n), bin(b)
		{

		}

		const unordered_set* parent;
		node*                curr;
		std::size_t          bin;
	};

	class const_local_iterator
	{
public:
		const_local_iterator()
			: curr(0), parent(0)
		{
		}

		const_local_iterator& operator++()
		{
			if ( curr )
			{
				curr = curr->next;
			}

			return *this;
		}

		const_reference operator*() const
		{
			return curr->value;
		}

		bool operator==(const const_local_iterator& o) const
		{
			return parent == o.parent && curr == o.curr;
		}

		bool operator!=(const const_local_iterator& o) const
		{
			return parent != o.parent || curr != o.curr;
		}

private:
		friend class unordered_set;

		const_local_iterator(
			const unordered_set* p,
			node*                n
		)
			: parent(p), curr(n)
		{

		}

		const unordered_set* parent;
		node*                curr;
	};

	typedef const_iterator iterator;
	typedef const_local_iterator local_iterator;

	unordered_set()
		: load(0)
	{
	}

	unordered_set(const unordered_set& o)
		: eq(o.eq), hash(o.hash), load(o.load)
	{
		const std::size_t n = o.bins.size();

		bins.resize(n);

		for ( std::size_t i = 0; i < n; ++i )
		{
			node* p = o.bins[i];

			while ( p )
			{
				node* q = p->next;

				bins[i] = new node(bins[i], p->hash, p->value);

				p = q;
			}
		}
	}

	unordered_set(
		size_type       n,
		const Hash&     hash_ = Hash(),
		const KeyEqual& equal = KeyEqual()
	)
		: eq(equal), hash(hash_), bins(n, 0), load(0)
	{

	}

	~unordered_set()
	{
		clear();
	}

	unordered_set& operator=(const unordered_set& o)
	{
		unordered_set t(o);

		swap(t);
		return *this;
	}

	bool empty() const
	{
		return load == 0;
	}

	size_type size() const
	{
		return load;
	}

	void clear()
	{
		for ( std::size_t i = 0, n = bins.size(); i < n; ++i )
		{
			node* p = bins[i];

			while ( p )
			{
				node* q = p->next;
				delete p;
				p = q;
			}

			bins[i] = 0;
		}

		load = 0;
	}

	void swap(unordered_set& o)
	{
		using std::swap;

		swap(bins, o.bins);
		swap(load, o.load);
		swap(eq, o.eq);
		swap(hash, o.hash);
	}

	const_iterator cbegin() const
	{
		const std::size_t n = bins.size();

		for ( std::size_t i = 0; i < n; ++i )
		{
			if ( bins[i] )
			{
				return const_iterator(this, bins[i], i);
			}
		}

		return cend();
	}

	const_iterator begin() const
	{
		return cbegin();
	}

	const_iterator cend() const
	{
		return const_iterator( this, 0, bins.size() );
	}

	const_iterator end() const
	{
		return cend();
	}

	size_type bucket(const value_type& v) const
	{
		return hash(v) % bins.size();
	}

	const_local_iterator cbegin(size_type b) const
	{
		if ( b < bins.size() )
		{
			return const_local_iterator(this, bins[b]);
		}

		return cend(b);
	}

	const_local_iterator cend(size_type b) const
	{
		return const_local_iterator(this, 0);
	}

	local_iterator begin(size_type b)
	{
		return cbegin(b);
	}

	local_iterator end(size_type b)
	{
		return cend(b);
	}

	const_iterator find(const value_type& v) const
	{
		const std::size_t n = bins.size();

		// Check if the value is already in the set
		if ( n != 0 )
		{
			const std::size_t h = hash(v);

			const std::size_t idx = h % n;

			node* p = bins[idx];

			while ( p )
			{
				if ( eq(p->value, v) )
				{
					return const_iterator(this, p, idx);
				}

				p = p->next;
			}
		}

		// Not found
		return cend();
	}

	size_type count(const value_type& v) const
	{
		return find(v) == cend() ? 0 : 1;
	}

	hasher hash_function() const
	{
		return hash;
	}

	key_equal key_eq() const
	{
		return eq;
	}

	bool insert(const value_type& v)
	{
		std::size_t n = bins.size();

		const std::size_t h = hash(v);

		std::size_t idx = 0;

		// Check if the value is already in the set
		if ( n != 0 )
		{
			idx = h % n;

			node* p = bins[idx];

			while ( p )
			{
				if ( eq(p->value, v) )
				{
					return false;
				}

				p = p->next;
			}
		}

		if ( load == n )
		{
			/* at maximum load factor. We're going to use the underlying growth
			 * of vector to decide our hash table size. Usually, the following
			 * line would trigger that.
			 */
			std::vector< node* > t(n + 1, 0);

			// grow to capacity
			n = t.capacity();
			t.resize(n);

			for ( std::size_t i = 0; i < load; ++i )
			{
				node* p = bins[i];

				while ( p )
				{
					const std::size_t j = p->hash % n;
					node* const       q = p->next;
					p->next = t[j];
					t[j]    = p;
					p       = q;
				}
			}

			bins.swap(t);
			idx = h % n;
		}

		// Do the insertion
		bins[idx] = new node(bins[idx], h, v);
		++load;
		return true;
	}

	size_type erase(const value_type& v)
	{
		const std::size_t n = bins.size();

		// Check if the value is already in the set
		if ( n != 0 )
		{
			const std::size_t h = hash(v);

			const std::size_t idx = h % n;

			if ( node* p = bins[idx] )
			{
				if ( eq(p->value, v) )
				{
					// first element must be handled specially
					bins[idx] = p->next;
					delete p;
					--load;
					return 1;
				}

				node* q = p;
				p = p->next;

				while ( p )
				{
					if ( eq(p->value, v) )
					{
						q->next = p->next;
						delete p;
						--load;
						return 1;
					}

					q = p;
					p = p->next;
				}
			}
		}

		return 0;
	}

	iterator erase(const_iterator pos)
	{
		if ( pos.parent == this && pos.curr )
		{
			if ( node* p = bins[pos.bin] )
			{
				const_iterator jt = pos;
				++jt;

				if ( p == pos.curr )
				{
					// Bin head
					bins[pos.bin] = p->next;
					delete p;
					--load;
					return jt;
				}

				node* q = p;
				p = p->next;

				while ( p )
				{
					if ( p == pos.curr )
					{
						q->next = p->next;
						delete p;
						--load;
						return jt;
					}

					q = p;
					p = p->next;
				}
			}
		}

		return cend();
	}

	size_type bucket_count() const
	{
		return bins.size();
	}

	float load_factor() const
	{
		if ( !bins.empty() )
		{
			return static_cast< float >( static_cast< double >( load ) / static_cast< double >( bins.size() ) );
		}

		return 0;
	}

	float max_load_factor() const
	{
		return 1;
	}

	void rehash(size_type n_)
	{
		size_type n = std::max( n_, std::max< size_type >( 8, static_cast< size_type >( load / max_load_factor() ) ) );

		std::vector< node* > t(n, 0);

		for ( std::size_t i = 0, m = bins.size(); i < m; ++i )
		{
			node* p = bins[i];

			while ( p )
			{
				const std::size_t j = p->hash % n;
				node* const       q = p->next;
				p->next = t[j];
				t[j]    = p;
				p       = q;
			}
		}

		bins.swap(t);
	}

private:
	key_equal            eq;
	hasher               hash;
	std::vector< node* > bins;
	std::size_t          load;
};
}

namespace std
{
template< typename K, typename H, typename E >
void swap(
	cpp11::unordered_set< K, H, E >& l,
	cpp11::unordered_set< K, H, E >& r
)
{
	l.swap(r);
}

}
#endif // ifdef CPP11

#endif // PBL_CPP_UNORDERED_SET_H
