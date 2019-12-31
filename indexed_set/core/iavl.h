#ifndef __base2_core_iavl__
#define __base2_core_iavl__

#include <inttypes.h>
#include <functional>
#include <exception>
#include <limits>
#include <tuple>
#include <iostream>
#include <chrono>

//#define AVLTREE_UNITTEST
#define CHECKED_BUILD

#ifndef ASSERT_THROW
#ifdef _DEBUG
#define ASSERT_THROW(b, x) if(!(b)) { throw std::exception(x); }
#else
#define ASSERT_THROW(b,x) {}
#endif
#endif

#include "./inode.h"


namespace indexed
{

	/*

		Single root, binary AVL tree, where elements can be addressed by index (uint32_t value).

		Actually, index values are just plain byte offsets into the main memory chunk,
		which, like in std::vector, is contiguous memory space that can be reallocated,
		when the growh is required.

		Erased elements are not 'deallocated', though the destructor is called properly, but rather kept in
		a chain of 'deleted' nodes, which can be reused during next insertion.

		Since entire tree with user values is in one chunk of memory, this memory can be serialized quickly or even stored,
		provided that user types are POD and are self-contained

	*/

	template <typename Tu, typename Tc = std::less<Tu>>
	struct _AvlTree : protected _Growable<>
	{
		using _Node = typename inode<Tu, Tc>;
		using _Iter = typename _Node::iterator;

		static constexpr size_t Nsize() { return sizeof(_Node); }

		using _Base = _Growable<>;


		//Construction
		_AvlTree() {}
		_AvlTree(const _AvlTree&) = default;
		_AvlTree(_AvlTree&&) = default;
		_AvlTree& operator=(const _AvlTree&) = delete;
		_AvlTree& operator=(_AvlTree&&) = delete;
		~_AvlTree() { Clear(); }





		//helpers
		inline _Node* N0() { return (_Node*)_Base::_Head(); }
		inline _Node* Nptr(off o) { return (_Node*)(_Base::_Head() + o); }
		inline _Node* NSafePtr(off o) { return o ? (_Node*)(_Base::_Head() + o) : nullptr; }
		inline Tu* Tptr(off o) { return &(Nptr(o)->payload); }
		inline off		Optr(_Node* p) { return (off)((u8*)p - _Base::_Head()); }



		//Iteration
		_Iter Begin() { return _Iter(NSafePtr(_Root)); }
		_Iter End() { return _Iter(); }

		/*
			Increases total capacity to store at least ElementCount elements of Tu
		*/
		void Reserve(size_t ElementCount) { _Base::_Reserve((u32)((ElementCount + 1) * Nsize())); }

		/*
			returns offset of the element and boolean flag meaning that this
			item really was inserted in this call (true) or already existed (false)
		*/
		std::pair<off, bool> Insert(const Tu& v)
		{
			_Node* n = nullptr;
			bool	added = true;

			if (!_Base::len_)
			{
				//first slot (root for deleted chain) is added only once
				_Base::_PtrAppendZeroBytes((u32)(Nsize()));
			}

			if (_Root)
			{
				auto [pnode, dir] = Nptr(_Root)->_InsertionPointFor<Tc>(v);

				if (dir == Dir::None)
				{
					n = pnode;
					added = false;
				}
				else
				{
					off parent = Optr(pnode);
					n = _CreateNode(v);
					Nptr(parent)->AddChild(n, dir);

					//any insertion can displace the root node by no more that one click
					_Root += Nptr(_Root)->parent;
				}
			}
			else
			{
				_Root = Optr(n = _CreateNode(v));
			}

			if (added)
				++_Cnt;

			return { Optr(n), added };
		}


		inline _Node* Root() { return (_Node*)(_Base::_Head() + _Root); }
		inline size_t	Size() const { return _Cnt; }

		void Clear()
		{
			if (auto R = NSafePtr(_Root))
			{
				R->DestroyRecursive();
				_Root = 0;
				_Cnt = 0;
			}

			_Base::_Reset();
		}

		void Foreach(std::function<void(const Tu&)> cb)
		{
			if (auto R = NSafePtr(_Root))
			{
				R->Inorder(cb);
			}
		}

#ifdef CHECKED_BUILD
		void __LifeCheck(std::ostream& o)
		{
			u32 mi = 0, ma = 0, lcnt = 0, total = 0;

			float tme = 0;

			if (_Root)
			{
				Nptr(_Root)->Enumerate([&](_Node* n)
					{
						++total;
						if (0 == n->left && 0 == n->right)
						{
							auto De = n->Depth();

							if (++lcnt == 1)
							{
								mi = ma = De;
							}
							else
							{
								mi = std::min<uint32_t>(mi, De);
								ma = std::max<uint32_t>(ma, De);
							}
						}
					});

			}

			o << "allocated memory: " << _Base::_Capacity() << std::endl;
			o << "   reallocations: " << _Base::Reallocs_ << std::endl;

			o << "     used memory: " << _Base::_Size() << std::endl;
			o << "total node count: " << total << std::endl;
			o << "      leaf nodes: " << lcnt << std::endl;
			o << "  min leaf depth: " << mi << std::endl;
			o << "  max leaf depth: " << ma << std::endl;

		}

#endif

		void Erase(const Tu& v)
		{
			if (auto Proot = NSafePtr(_Root))
			{
				if (auto [pnode, dir] = Proot->_InsertionPointFor<Tc>(v); dir == Dir::None)
				{
					if (_Node::_EraseNode(Proot, pnode))
					{
						--_Cnt;

						_Root = Proot ? Optr(Proot) : 0;

						_Node::_DecommissionNode(pnode, N0());
					}
				}
			}
		}
		void EraseAtOffset(off o)
		{
			if (auto Proot = NSafePtr(_Root))
			{
				if (auto pnode = NSafePtr(o))
				{
					if (_Node::_EraseNode(Proot, pnode))
					{
						--_Cnt;

						_Root = Proot ? Optr(Proot) : 0;

						_Node::_DecommissionNode(pnode, N0());
					}
				}
			}
		}

		_Iter FindNode(const Tu& v)
		{
			_Node* Found = nullptr;

			if (auto Proot = NSafePtr(_Root))
			{
				if (auto [n, d] = Proot->_InsertionPointFor<Tc>(v); d == Dir::None)
				{
					Found = n;
				}
			}

			return _Iter::from_node(Found);
		}

	protected:

		//returns 'deleted' node if present, or appends a new one
		inline _Node* _CreateNode(const Tu& v)
		{
			_Node* n = N0()->NSafeRight();
			if (n)
			{
				//dequeue
				N0()->right = n->right ? (N0()->right + n->right) : 0;

				n->right = 0;
				n->tilt = Dir::None;
			}
			else
			{
				n = (_Node*)_Base::_PtrAppendZeroBytes((u32)Nsize());
				n->tilt = Dir::None;
			}

			new (n)  Tu(v);

			return n;
		}

	protected:
		u32		_Cnt = { 0 };
		off		_Root = { 0 };
	};



	typedef uint32_t slot;


	/*
		Represents a set, based on AVL tree, where inserted elements can be addressed by slot index or by value.

		Slot numbers are in [1...N] where N has no relation of actual number of elements in the set.
		This can happen because slots for 'erased' elements can later be reused to store 'inserted' elements

		So, elements in this set can be reached in two ways:
		1. Binary search 'by value', the same as in std::set
		2. By slot, indexed way, similar in std::vector

	*/
	template <typename Tu, typename Tc = std::less<Tu>>
	struct set : protected _AvlTree<Tu, Tc>
	{
		static_assert(std::is_trivially_copyable<Tu>::value);

		using _Base = typename _AvlTree<Tu, Tc>;
		using iter = typename _AvlTree<Tu, Tc>::_Iter;

		static constexpr slot ToSlot(off o) { return o ? (slot)(size_t(o) / _Base::Nsize()) : 0; }


		set(size_t initialCount = 0) { if (initialCount) _Base::Reserve(initialCount); }

		set(const set&) = default;
		set(set&&) = default;
		set& operator=(const set&) = delete;
		set& operator=(set&&) = delete;


		inline size_t	size() const { return _Base::Size(); }
		inline bool		empty() const { return 0 == size() ? true : false; }

		inline void		reserve(size_t count) { _Base::_Reserve((uint32_t)((count+1) * _Base::Nsize())); }

		/* returns slot number for specified value and boolean flag, indicating that a given value was actually inserted */
		std::pair<slot, bool> insert(const Tu& v)
		{
			auto [o, added] = _Base::Insert(v);
			return  { ToSlot(o), added };
		}

		/* returns value that's owned by the set, either inserted or existing */
		std::pair<const Tu&, slot> inserted(const Tu& v)
		{
			auto [o, added] = _Base::Insert(v);
			return { *_Base::Tptr(o), ToSlot(o) };
		}

		void erase(const Tu& v) { _Base::Erase(v); }
		void erase_at(slot pos) { _Base::EraseAtOffset((off)(pos * _Base::Nsize())); }

		slot operator[](const Tu& v)
		{
			auto [o, added] = _Base::Insert(v);
			return ToSlot(o);
		}

		iter find(const Tu& v)
		{
			return _Base::FindNode(v);
		}

		slot find_slot(const Tu& v)
		{
			slot o = 0;
			if (auto it = _Base::FindNode(v))
			{
				o = ToSlot(_Base::Optr(it.node()));
			}
			return o;
		}



		inline const Tu& at(slot pos)
		{
			return *_Base::Tptr((off)(pos * _Base::Nsize()));
		}

		iter begin() { return _Base::Begin(); }
		iter end() { return _Base::End(); }


		void clear() { _Base::Clear(); }

		void foreach(std::function<void(const Tu & v)> f) { _Base::Foreach(f); }


#ifdef CHECKED_BUILD
		void dbg_report(std::ostream& o = std::cout)
		{
			_Base::__LifeCheck(o);
		}
		bool dbg_validate()
		{
			return _Base::__ValidateIntegrity();
		}
#endif
	};

}
#endif

