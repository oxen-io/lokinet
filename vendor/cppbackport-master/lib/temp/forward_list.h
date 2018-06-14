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
#ifndef PBL_CPP_FORWARD_LIST_H
#define PBL_CPP_FORWARD_LIST_H

#include "version.h"

#ifdef CPP11
#include <forward_list>
#else
#include <cstddef>
#include <limits>

namespace cpp11
{
/** Implementation of std::forward_list
 *
 * @todo Some of these functions duplicate logic (or nearly so)
 */
template< typename T >
class forward_list
{
	struct node
	{
		node* next;
	};

	struct value_node
		: node
	{
		value_node()
			: value()
		{
		}

		explicit value_node(const T& value_)
			: value(value_)
		{
		}

		T value;
	};
public:
	typedef T value_type;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;
	typedef value_type& reference;
	typedef const value_type& const_reference;
	typedef T* pointer;
	typedef const T* const_pointer;

	class iterator
	{
public:
		iterator()
			: p(0)
		{
		}

		explicit iterator(node* p_)
			: p(p_)
		{
		}

		iterator& operator++()
		{
			p = p->next;

			return *this;
		}

		iterator operator++(int)
		{
			iterator t(*this);

			p = p->next;

			return t;
		}

		pointer operator->()
		{
			return &( static_cast< value_node* >( p )->value );
		}

		const_pointer operator->() const
		{
			return &( static_cast< const value_node* >( p )->value );
		}

		reference operator*()
		{
			return static_cast< value_node* >( p )->value;
		}

		const_reference operator*() const
		{
			return static_cast< const value_node* >( p )->value;
		}

		bool operator==(const iterator& jt) const
		{
			return p == jt.p;
		}

		bool operator!=(const iterator& jt) const
		{
			return p != jt.p;
		}

private:
		friend class forward_list;
		friend class const_iterator;

		node* p;
	};

	class const_iterator
	{
public:
		const_iterator()
			: p(0)
		{
		}

		const_iterator(const iterator& it)
			: p(it.p)
		{
		}

		explicit const_iterator(node* p_)
			: p(p_)
		{
		}

		const_iterator& operator++()
		{
			p = p->next;

			return *this;
		}

		const_iterator operator++(int)
		{
			const_iterator t(*this);

			p = p->next;

			return t;
		}

		const_pointer operator->() const
		{
			return &( static_cast< const value_node* >( p )->value );
		}

		const_reference operator*() const
		{
			return static_cast< const value_node* >( p )->value;
		}

		bool operator==(const const_iterator& jt) const
		{
			return p == jt.p;
		}

		bool operator!=(const const_iterator& jt) const
		{
			return p != jt.p;
		}

private:
		friend class forward_list;
		node* p;
	};

	forward_list()
	{
		p.next = 0;
	}

	forward_list(
		size_type count,
		const T&  value
	)
	{
		node* q = &p;

		for (; count != 0; --count )
		{
			q->next = new value_node(value);
			q       = q->next;
		}

		q->next = 0;
	}

	forward_list(size_type count)
	{
		node* q = &p;

		for (; count != 0; --count )
		{
			q->next = new value_node;
			q       = q->next;
		}

		q->next = 0;
	}

	template< typename InputIt >
	forward_list(
		InputIt first,
		InputIt last
	)
	{
		node* q = &p;

		for (; first != last; ++first )
		{
			q->next = new value_node(*first);
			q       = q->next;
		}

		q->next = 0;
	}

	forward_list(const forward_list& l)
	{
		node* q = &p;

		for ( const node* first = l.p.next; first; first = first->next )
		{
			q->next = new node(first->value);
			q       = q->next;
		}

		q->next = 0;
	}

	~forward_list()
	{
		clear();
	}

	forward_list& operator=(const forward_list& l)
	{
		forward_list t(l);

		swap(t);

		return *this;
	}

	void assign(
		size_type count,
		const T&  value
	)
	{
		forward_list t(count, value);

		swap(t);
	}

	template< typename InputIt >
	void assign(
		InputIt first,
		InputIt last
	)
	{
		forward_list t(first, last);

		swap(t);
	}

	reference front()
	{
		return p.next->value;
	}

	const_reference front() const
	{
		return p.next->value;
	}

	bool empty() const
	{
		return p.next == 0;
	}

	size_type max_size() const
	{
		return std::numeric_limits< size_type >::max();
	}

	void clear()
	{
		node* q = p.next;

		while ( q )
		{
			node* t = q;
			q = q->next;
			delete t;
		}

		p.next = 0;
	}

	iterator before_begin()
	{
		return iterator(&p);
	}

	const_iterator before_begin() const
	{
		return const_iterator(&p);
	}

	const_iterator cbefore_begin() const
	{
		return const_iterator(&p);
	}

	iterator begin()
	{
		return iterator(p.next);
	}

	const_iterator begin() const
	{
		return const_iterator(p.next);
	}

	const_iterator cbegin() const
	{
		return const_iterator(p.next);
	}

	iterator end()
	{
		return iterator(0);
	}

	const_iterator end() const
	{
		return const_iterator(0);
	}

	const_iterator cend() const
	{
		return const_iterator(0);
	}

	iterator insert_after(
		const_iterator pos,
		const T&       value
	)
	{
		if ( node* q = pos.p )
		{
			node* t = new value_node(value);
			t->next = q->next;
			q->next = t;

			return iterator(t);
		}

		return iterator(0);
	}

	iterator insert_after(
		const_iterator pos,
		size_type      count,
		const T&       value
	)
	{
		if ( count != 0 )
		{
			return iterator( insert_after_private(pos.p, count, value) );
		}

		return iterator(pos.p);
	}

	template< typename InputIt >
	iterator insert_after(
		const_iterator pos,
		InputIt        first,
		InputIt        last
	)
	{
		node* q = pos.p;

		if ( q )
		{
			for (; first != last; ++first )
			{
				node* t = new value_node(*first);
				t->next = q->next;
				q->next = t;
				q       = t;
			}
		}

		return iterator(q);
	}

	iterator erase_after(const_iterator pos)
	{
		if ( node* q = pos.p )
		{
			if ( node* t = q->next )
			{
				q->next = t->next;
				delete t;

				return iterator(q->next);
			}
		}

		return iterator(0);
	}

	void push_front(const T& value)
	{
		node* t = new value_node(value);

		t->next = p.next;
		p.next  = t;
	}

	void pop_front()
	{
		if ( p.next )
		{
			node* t = p.next;
			p.next = t->next;
			delete t;
		}
	}

	void resize(size_type count)
	{
		std::pair< node*, size_type > rem = resize_private(count);

		if ( rem.second != 0 )
		{
			insert_after_private(rem.first, rem.second);
		}
	}

	void resize(
		size_type count,
		const T&  value
	)
	{
		std::pair< node*, size_type > rem = resize_private(count);

		if ( rem.second != 0 )
		{
			insert_after_private(rem.first, rem.second, value);
		}
	}

	void swap(forward_list& l)
	{
		std::swap(p.next, l.p.next);
	}

	void splice_after(
		const_iterator pos,
		forward_list&  other
	)
	{
		splice_after( pos, other, other.before_begin() );
	}

	void splice_after(
		const_iterator pos,
		forward_list&  other,
		const_iterator it
	)
	{
		if ( node* q = pos.p )
		{
			if ( node* r = it.p )
			{
				if ( node* u = r->next )
				{
					r->next = 0;
					node* t = q->next;
					q->next = u;

					if ( t )
					{
						while ( u->next )
						{
							u = u->next;
						}

						u->next = t;
					}
				}
			}
		}
	}

	void splice_after(
		const_iterator pos,
		forward_list&  other,
		const_iterator first,
		const_iterator last
	)
	{
		if ( node* q = pos.p )
		{
			if ( node* r = first.p )
			{
				if ( node* u = r->next )
				{
					r->next = last.p;
					node* t = q->next;
					q->next = u;

					while ( u->next != last.p )
					{
						u = u->next;
					}

					u->next = t;
				}
			}
		}
	}

	void remove(const T& value)
	{
		node* q = &p;

		while ( q->next )
		{
			if ( static_cast< value_node* >( q->next )->value == value )
			{
				node* t = q->next;
				q->next = t->next;
				delete t;
			}
			else
			{
				q = q->next;
			}
		}
	}

	template< typename Pred >
	void remove_if(Pred pred)
	{
		node* q = &p;

		while ( q->next )
		{
			if ( pred(static_cast< value_node* >( q->next )->value) )
			{
				node* t = q->next;
				q->next = t->next;
				delete t;
			}
			else
			{
				q = q->next;
			}
		}
	}

	void reverse()
	{
		node* q = p.next;
		node* r = 0;

		while ( q )
		{
			node* t = q->next;
			q->next = r;
			r       = q;
			q       = t;
		}

		p.next = r;
	}

	void unique()
	{
		unique(equal_to);
	}

	template< typename Predicate >
	void unique(Predicate pred)
	{
		node* q = p.next;

		while ( q )
		{
			if ( node* t = q->next )
			{
				if ( pred(static_cast< value_node* >( q )->value, static_cast< value_node* >( t )->value) )
				{
					q->next = t->next;
					delete t;
				}
				else
				{
					q = t;
				}
			}
			else
			{
				break;
			}
		}
	}

	void merge(forward_list& other)
	{
		merge(other, less_than);
	}

	template< typename Compare >
	void merge(
		forward_list& other,
		Compare       comp
	)
	{
		p.next       = merge_inner(p.next, other.p.next, comp);
		other.p.next = 0;
	}

	void sort()
	{
		sort(less_than);
	}

	template< typename Compare >
	void sort(Compare comp)
	{
		p.next = sort_inner(p.next, comp);
	}

private:
	static bool equal_to(
		const T& a,
		const T& b
	)
	{
		return a == b;
	}

	static bool less_than(
		const T& a,
		const T& b
	)
	{
		return a < b;
	}

	/** Split list in two, sort each half, merge, return the first node
	 *
	 * @todo Can probably be done without recursion. Sort 2 items, 2, 4, 2, 2,
	 * 4, 8
	 * @todo Probably don't need to traverse the list multiple times
	 */
	template< typename Compare >
	static node* sort_inner(
		node*   q,
		Compare comp
	)
	{
		if ( q && q->next )
		{
			node* turtle = q;
			node* hare   = q;

			while ( hare->next && hare->next->next )
			{
				turtle = turtle->next;
				hare   = hare->next->next;
			}

			hare         = turtle->next;
			turtle->next = 0;
			return merge_inner(sort_inner(q, comp), sort_inner(hare, comp), comp);
		}

		return q;
	}

	/** Build a third list by repeatedly taking the least of the two given lists
	 *
	 * @todo Could probably save some rewriting of each s->next by following
	 * an algorithm like set_union
	 */
	template< typename Compare >
	static node* merge_inner(
		node*   q,
		node*   r,
		Compare comp
	)
	{
		node  t = { 0 };
		node* s = &t;

		while ( q && r )
		{
			if ( comp(static_cast< value_node* >( r )->value, static_cast< value_node* >( q )->value) )
			{
				s->next = r;
				r       = r->next;
			}
			else
			{
				s->next = q;
				q       = q->next;
			}

			s = s->next;
		}

		s->next = q ? q : r;

		return t.next;
	}

	// count must not be zero
	node* insert_after_private(
		node*     q,
		size_type count
	)
	{
		if ( q )
		{
			node* t = q->next;

			for (; count != 0; --count )
			{
				node* u = new value_node;
				u->next = t;
				t       = u;
			}

			q->next = t;
		}

		return q;
	}

	node* insert_after_private(
		node*     q,
		size_type count,
		const T&  value
	)
	{
		if ( q )
		{
			node* t = q->next;

			for (; count != 0; --count )
			{
				node* u = new value_node(value);
				u->next = t;
				t       = u;
			}

			q->next = t;
		}

		return q;
	}

	std::pair< node*, size_type > resize_private(size_type count)
	{
		node* q = &p;

		while ( q->next )
		{
			q = q->next;

			if ( --count == 0 )
			{
				node* t = q->next;
				q->next = 0;

				while ( t )
				{
					q = t->next;
					delete t;
					t = q;
				}

				return std::pair< node*, size_type >(0, 0);
			}
		}

		return std::pair< node*, size_type >(q, count);
	}

	node p;
};
}
#endif // ifdef CPP11

#endif // FORWARD_LIST_H
