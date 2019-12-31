#ifndef __base2_core_inode__
#define __base2_core_inode__

#ifndef ASSERT_THROW
#ifdef _DEBUG
#define ASSERT_THROW(b, x) if(!(b)) { throw std::exception(x); }
#else
#define ASSERT_THROW(b,x) {}
#endif
#endif

#include <algorithm>
#include <functional>
#include <iostream>

namespace indexed
{

#pragma region Common types ...
	typedef int32_t		off; //offset, relative to some point in memory

/*
	Direction (or balance) of a tree node.
*/
	enum struct Dir : uint8_t
	{
		Left = 1, None = 2, Right = 3
	};

	/*
		Represents a pair of consecutive directions, used in complex rotations
	*/
	enum struct Dir2 : uint8_t
	{
		LeftLeft = 0b0101,
		LeftRight = 0b0111,
		RightLeft = 0b1101,
		RightRight = 0b1111

		//remaining combinations are 'invalid'
	};


	inline Dir operator~(Dir d) { return d == Dir::Left ? Dir::Right : Dir::Left; }

	//'no-direction', true when the node is in-balance, false when not (has tilt)
	inline bool operator!(Dir d) { return d == Dir::None ? true : false; }

	//for pipelining (a concatenation) of two consecutive rotation
	inline Dir2 operator|(Dir a, Dir b) { return (Dir2)((((uint8_t)a) << 2) + (uint8_t)b); }
#pragma endregion

#pragma region Growable ...
	/*
		This growable array operates on bytes only
		Growth happens by at least MIN_GROW_BY bytes or at most requested count, aligned by Ta size
	*/
	template <uint32_t MIN_GROW_BY = 1024, uint32_t Ta = 16>
	struct _Growable
	{
		static_assert(MIN_GROW_BY > 0);
		static_assert(((~(Ta - 1))& Ta) == Ta, "ERR: Only one bit must be set in aligment size");

		static constexpr uint32_t Aligned(uint64_t n) { return (n + (Ta - 1)) & (~(Ta - 1)); }

		using u8 = uint8_t;
		using u32 = uint32_t;


		_Growable() {}
		_Growable(const _Growable& o)
		{
			if (o.ptr_)
			{
				ptr_ = (u8*)malloc(o.capacity_);
				memcpy(ptr_, o.ptr_, o.capacity_);

				len_ = o.len_;
				capacity_ = o.capacity_;
			}
		}

		_Growable(_Growable&& o)
		{
			if (o.ptr_)
			{
				ptr_ = o.ptr_;
				len_ = o.len_;
				capacity_ = o.capacity_;

				o.ptr_ = nullptr;
				o.len_ = o.capacity_ = 0;
			}
		}
		_Growable& operator=(const _Growable&) = delete;
		_Growable& operator=(_Growable&&) = delete;

		~_Growable() { _Reset(); }


		inline u32	_Size() const { return len_; }
		inline u32	_Capacity() const { return capacity_; }

		inline u8* _Head() { return ptr_; }
		inline const u8* _Head() const { return ptr_; }


		inline u8* _PtrOf(u32 offset) { return ptr_ + offset; }
		inline const u8* _PtrOf(u32 offset) const { return ptr_ + offset; }

		template <typename To>
		To* _AsPtrOf(u32 offset) { return (To*)(ptr_ + offset); }

		template <typename To>
		const To* _AsPtrOf(u32 offset) const { return (To*)(ptr_ + offset); }




		/*
			all '_PtrAppend* functions will return a pointer to just inserted sequence of bytes, typed or not.

			Note that this pointer is only valid until the next buffer reallocation and should not be kept

		*/
#pragma region Append -> Ptr ...

		u8* _PtrAppendBytes(const void* src, u32 cnt)
		{
			u8* _At = _GrowBy(cnt);

			memcpy(_At, src, cnt);

			len_ += (u32)cnt;

			return _At;
		}

		u8* _PtrAppendZeroBytes(u32 cnt)
		{
			u8* _At = _GrowBy(cnt);
			len_ += (u32)cnt;
			return _At;
		}

		char* _PtrAppendCstring(const char* p)
		{
			char* o = nullptr;

			if (!p || !*p)
			{
				//adding a single byte - zero-terminator - and return its address
				u8 _Zero = 0;
				o = (char*)_PtrAppendBytes(&_Zero, 1);
			}
			else
			{
				o = (char*)_PtrAppendBytes(p, strlen(p) + 1);
			}
			return o;
		}

		template <typename To>
		To* _PtrAppend(const To& v)
		{
			static_assert(std::is_trivially_copyable<To>::value);
			return (To*)_PtrAppendBytes(&v, sizeof(To));
		}

		/* appends sizeof(To) bytes cleared to zero and returns typed pointer */
		template <typename To>
		To* _PtrAppendBlank()
		{
			return (To*)_PtrAppendZeroBytes(sizeof(To));
		}

#pragma endregion


	protected:

		/* releases allocated memory */
		void _Reset()
		{
			if (ptr_)
			{
				free(ptr_);
				ptr_ = nullptr;
				len_ = capacity_ = 0;
			}
		}

		void _Reserve(u32 totalBytes)
		{
			if (totalBytes > capacity_)
			{
				_GrowBy(totalBytes - len_);
			}
		}

		/* Always returns a pointer where bytes can be appended. In case there is no memory available throws OutOfMemory */
		u8* _GrowBy(u32 bytesToAdd)
		{
			if ((capacity_ - len_) < bytesToAdd)
			{
				u32 _NewCap = Aligned(std::max<uint64_t>({ (bytesToAdd - (capacity_ - len_)), MIN_GROW_BY, capacity_ / 2 }) + capacity_);

				u8* _Moved = (u8*)malloc(_NewCap);

				if (!_Moved)
					throw std::bad_alloc();

				//copy old stuff
				if (len_)
				{
					memcpy(_Moved, ptr_, len_);
				}

				//zeroize added chunk
				if (_NewCap > len_)
				{
					memset(_Moved + len_, 0, _NewCap - len_);
				}

				//clear
				if (ptr_)
				{
					free(ptr_);
					ptr_ = nullptr;
				}

				//new values
				ptr_ = _Moved;
				capacity_ = _NewCap;

#ifdef CHECKED_BUILD
				++Reallocs_;
#endif

			}

			return ptr_ + len_;
		}

		/*
			Adds memory to place specified number of objects and returns its address as a typed pointer
			allocated memory is zeroized
		*/
		template <typename To>
		To* _GrowBy(u32 Count = 1)
		{
			return (To*)_GrowBy(Count * sizeof(To));
		}



	protected:

		u8* ptr_ = { nullptr };
		u32		len_ = { 0 }, capacity_ = { 0 };

#ifdef CHECKED_BUILD
		u32		Reallocs_ = { 0 };
#endif // CHECKED_BUILD


	};

#pragma endregion



#pragma region INODE ...
	/*
		Node for binary tree.

		Keeps references of its parent, and two immediate chilren - left and right.
		Reference is a signed byte distance from 'this' to another node

		For now using 32-bit values to store these references, so no more than 2G distance is allowed

		Carried type Tu must implement copy-constructor, Tu destructor is properly called when the node is deleted.

	*/
	template <typename Tu, typename Tc = std::less<Tu>>
	struct inode
	{
		using u8 = typename uint8_t;
		using u32 = typename uint32_t;

		Tu payload;

		mutable off parent, left, right;
		mutable Dir tilt;

		//these two are currently used for alignment only and are not used by this object in any way
		uint8_t		tag8;
		uint16_t	tag16;


		typedef inode* nptr;
		typedef const inode* nptr_c;

		/*
			'true' for active nodes, 'false' for non-initialized or deleted nodes
		*/
		inline bool IsEmpty() const { return (0 == ((u8)tilt)) ? true : false; }

		//Non-safe pointers
		inline nptr			_Ptr(off o) { return (nptr)((u8*)this + o); }
		inline nptr_c		_Ptr(off o) const { return (nptr)((u8*)this + o); }
		inline nptr			Nleft() { return _Ptr(left); }
		inline nptr			Nright() { return _Ptr(right); }
		inline nptr			Nparent() { return _Ptr(parent); }

		inline static constexpr off _Off(nptr a, nptr b) { return (off)((u8*)b - (u8*)a); }

		//Safe pointers
		inline nptr			NSafeParent() { return parent ? _Ptr(parent) : nullptr; }
		inline nptr_c		NSafeParent() const { return parent ? _Ptr(parent) : nullptr; }

		inline nptr			NSafeLeft() { return left ? _Ptr(left) : nullptr; }
		inline nptr_c		NSafeLeft() const { return left ? _Ptr(left) : nullptr; }

		inline nptr			NSafeRight() { return right ? _Ptr(right) : nullptr; }
		inline nptr_c		NSafeRight() const { return right ? _Ptr(right) : nullptr; }

		inline Dir		Branch() { return parent ? (Nparent()->left == (-parent) ? Dir::Left : Dir::Right) : Dir::None; }

		inline nptr	Root()
		{
			nptr n = this;
			while (n->parent)
			{
				n = n->Nparent();
			}
			return n;
		}

		/* returns 'heaviest' child, which carries a longest branch */
		inline nptr	Nheavy()
		{
			return tilt == Dir::Left ? _Ptr(left) : _Ptr(right);
		}

		void Inorder(std::function<void(const Tu&)> cb)
		{
			if (left)
				Nleft()->Inorder(cb);

			cb(payload);

			if (right)
				Nright()->Inorder(cb);
		}

		void Enumerate(std::function<void(nptr)> cb)
		{
			if (left)
				Nleft()->Enumerate(cb);

			if (right)
				Nright()->Enumerate(cb);

			cb(this);

		}

		u32 Depth() const
		{
			u32 d = 0;
			nptr_c n = this;

			while (n && n->parent)
			{
				++d;
				n = n->NSafeParent();
			}

			return d;
		}

		inline void __DestroyPayload()
		{
			payload.~Tu();
		}

		void DestroyRecursive()
		{
			__DestroyPayload();

			if (left)
				Nleft()->DestroyRecursive();
			if (right)
				Nright()->DestroyRecursive();
		}


		inline void AddChild(nptr child, Dir where)
		{
			ASSERT_THROW(child, "Cannot add null child");

			(where == Dir::Left ? left : right) = _Off(this, child);
			child->parent = _Off(child, this);

			Retrace_Insert(this, where);
		}

		static void Retrace_Insert(nptr n, Dir added)
		{
			while (n)
			{
				if (Dir::None == n->tilt)
				{
					n->tilt = added;
					added = n->Branch();
				}
				else
				{
					if (n->tilt != added)
					{
						//counter-balanced, this node becomes 'in balance', but the tree balance above it will not change
						n->tilt = Dir::None;
					}
					else
					{
						_Rotate_Insert(n);
					}

					break;
				}

				n = n->NSafeParent();

			}
		}



		/*
			Called on the node that needs to be rotated after one of its grandchildren branches became longer, which
			happened on this node's already heavy branch

			ASSUMPTIONS:
				- this node has both child and grandchild on 'heavy' direction
				- tilt of this node is either left or right
				- tilt of the heaviest child, which must exist, is either left or right

			The combination of these two tilts define the type of rotation applied.

			The tilt of 3 involved nodes will be adjusted accordingly.

			All 4 types of rotation cause the returned node to become balanced, which cannot affect balance of any node above this

		*/
		static void _Rotate_Insert(nptr Z)
		{
			nptr Y = Z->Nheavy();
			nptr X = Y->Nheavy();

			switch (Z->tilt | Y->tilt)
			{
			case Dir2::LeftLeft:
			{
				_Rotate_LL(Z, Y, X);

				/* Y and Z become balanced, X keeps its original balance */
				Z->tilt = Y->tilt = Dir::None;
			}
			break;

			case Dir2::RightRight:
			{
				_Rotate_RR(Z, Y, X);

				/* Y and Z become balanced, X keeps its original balance */
				Z->tilt = Y->tilt = Dir::None;
			}
			break;

			case Dir2::LeftRight:
			{
				_Rotate_LR(Z, Y, X);

				//balance
				Y->tilt = (X->tilt == Dir::Right) ? Dir::Left : Dir::None;
				Z->tilt = (X->tilt == Dir::Left) ? Dir::Right : Dir::None;
				X->tilt = Dir::None;
			}
			break;

			case Dir2::RightLeft:
			{
				_Rotate_RL(Z, Y, X);

				//balance?
				Y->tilt = (X->tilt == Dir::Left) ? Dir::Right : Dir::None;
				Z->tilt = (X->tilt == Dir::Right) ? Dir::Left : Dir::None;
				X->tilt = Dir::None;

			}
			break;

			default:
				break;
			}
		}

		static nptr _Rotate_Erase(nptr Z)
		{
			nptr Y = Z->Nheavy();
			nptr X = nullptr, o = nullptr;

			/*
				TODO: if Y is balanced, we can pick X on either side of Y.

				Figure out which pick is beneficial in terms of prodcuing more balanced tree or stopping
				repair process faster (can we have both?)

				Do we need randomness in making a choise here?


				1. if in case of equal Y we choose LL/RR case it's always a stop in repair, because
					the node Y, which replaces Z, will always be counterbalanced to Z, and Z and X keep their original tilt

					If non-tilted Y is converted to RL/LR case, then X being counterbalanced to Z produces error in form of
					out-of-balance Y node, which would require another rotation.

				So, for now, in-balance node Y is always treated as LL/RR case

			*/

			X = !Y->tilt ? (Z->tilt == Dir::Left ? Y->NSafeLeft() : Y->NSafeRight()) : Y->Nheavy();

			switch (Z->tilt | X->Branch())
			{
			case Dir2::LeftLeft:
			{
				_Rotate_LL(Z, Y, X);

				//X - not changed
				if (!Y->tilt)
				{
					//forced RR case, Y was in-balance and becomes Left, ZX not changed
					//returning null because repair is done
					Y->tilt = Dir::Right;
				}
				else
				{
					(o = Y)->tilt = Z->tilt = Dir::None;
				}
			}
			break;

			case Dir2::RightRight:
			{
				//Parent(Z) is wired with Y
				if (auto P = Z->NSafeParent())
				{
					Y->parent += Z->parent;
					((P->left == -(Z->parent)) ? P->left : P->right) += Z->right;
				}
				else
				{
					Y->parent = 0;
				}

				//Y becomes Z's parent
				Z->parent = Z->right;

				//T2 (former Left(Y)) is reconnected to Z on the right
				if (Y->left)
				{
					Z->right = Z->right + Y->left;
					Z->Nright()->parent = -(Z->right);
				}
				else
					Z->right = 0;

				//Left(Y) is repointed to Z
				Y->left = -(Z->parent);

				//X - not changed
				if (!Y->tilt)
				{
					//forced RR case, Y was in-balance and becomes Left, ZX not changed
					//returning null because repair is done
					Y->tilt = Dir::Left;
				}
				else
				{
					(o = Y)->tilt = Z->tilt = Dir::None;
				}
			}
			break;
			case Dir2::LeftRight:
			{
				_Rotate_LR(Z, Y, X);

				//balance
				Y->tilt = (X->tilt == Dir::Right) ? Dir::Left : Dir::None;
				Z->tilt = (X->tilt == Dir::Left) ? Dir::Right : Dir::None;
				(o = X)->tilt = Dir::None;

			}
			break;
			case Dir2::RightLeft:
			{
				_Rotate_RL(Z, Y, X);

				//balance?
				Y->tilt = (X->tilt == Dir::Left) ? Dir::Right : Dir::None;
				Z->tilt = (X->tilt == Dir::Right) ? Dir::Left : Dir::None;
				(o = X)->tilt = Dir::None;
			}
			break;

			default:
				ASSERT_THROW(false, "Invalid insert-rotate case");
				break;
			}


			return o;

		}

		static void Retrace_Erase(nptr n, Dir del)
		{
			ASSERT_THROW(del != Dir::None, "Incorrect deletion branch");
			while (n)
			{
				if (!n->tilt)
				{
					//this node is in balance, making 'del' branch shorter will not change
					//height of the tree above this node, so this node ends the cycle
					n->tilt = ~del;
					break;
				}
				else if (n->tilt == del)
				{
					//node was deleted on the branch that was heavier, this node now
					//becomes 'balanced', need to continue up the chain
					n->tilt = Dir::None;
				}
				else
				{
					//node was already 'heavy' in opposite direction and needs to be rotated,
					//after rotation it will become balanced and the cycle should continue up
					if (!(n = _Rotate_Erase(n)))

						break;
					//TODO: add rotation
				}

				//go up one level
				del = n->Branch();
				n = n->NSafeParent();

			};
		}


		/* geiven node will be wiped out and connected on the right of DelChain node */
		static void _DecommissionNode(nptr n, nptr Del)
		{
			ASSERT_THROW(n && Del, "Both nodes must be present for decommissioning");
			memset(n, 0, sizeof(*n));

			/*
				NOTE: chain of delete nodes is connected only on the 'right'
			*/
			if (Del->right)
			{
				n->right = _Off(n, Del) + Del->right;
			}

			Del->right = _Off(Del, n);
		}
		static bool _EraseNode(nptr& outRoot, nptr n)
		{
			if (!n || n->IsEmpty())
				return false;

			//
			// Case with both children present is replaced by swap and reduced to 0|1 children case
			//
			if (n->left && n->right)
			{
				nptr swap = nullptr;
				if (n->tilt == Dir::Right)
				{
					swap = n->Nright();
					while (swap->left)
						swap = swap->Nleft();
				}
				else
				{
					swap = n->Nleft();
					while (swap->right)
						swap = swap->Nright();
				}

				n->_SwapWith(swap);
			}

			nptr P = n->NSafeParent(), T1 = n->NSafeLeft(), T2 = n->NSafeRight();

			//
			// After potential reduction we delete a node with 1 or 0 children
			//
			if (P)
			{
				if (T1 && T2)
				{
					ASSERT_THROW(false, "Should not happen");
				}
				else if (T1)
				{
					//T1 is present, T2 is missing
					//T1 is reconnected to the Parent
					T1->parent = _Off(T1, P);
					if (P->left == -(n->parent))
					{
						P->left = _Off(P, T1);

						Retrace_Erase(P, Dir::Left);
					}
					else
					{
						P->right = _Off(P, T1);
						Retrace_Erase(P, Dir::Right);
					}
				}
				else if (T2)
				{
					//T1 is mising, T2 is present
					//T2 is reconnected to the parent
					T2->parent = _Off(T2, P);
					if (P->left == -(n->parent))
					{
						P->left = _Off(P, T2);
						Retrace_Erase(P, Dir::Left);
					}
					else
					{
						P->right = _Off(P, T2);
						Retrace_Erase(P, Dir::Right);
					}
				}
				else
				{
					//T1 and T2 both missing, Parent is disconnected from n
					if (P->left == -(n->parent))
					{
						P->left = 0;
						Retrace_Erase(P, Dir::Left);
					}
					else
					{
						P->right = 0;
						Retrace_Erase(P, Dir::Right);
					}
				}

				outRoot = P->Root();
			}
			else
			{
				//no parent
				if (T1 && T2)
				{
					ASSERT_THROW(false, "Two-child case out of place");
				}
				else if (T1)
				{
					//T1 present, T2 is missing
					//T1 becomes root
					(outRoot = T1)->parent = 0;
				}
				else if (T2)
				{
					//T2 present, T1 is missing
					//T2 becomes root
					(outRoot = T2)->parent = 0;
				}
				else
				{
					//last node in the chain
					outRoot = nullptr;
				}
			}

			n->parent = n->left = n->right = 0;

			n->__DestroyPayload();

			return true;
		}


		static nptr LeftmostOf(nptr n)
		{
			while (n && n->left)
			{
				n = n->Nleft();
			}
			return n;
		}

		/* returns next 'in order' element or null if nowhere to go */
		static nptr InorderNextOf(nptr n)
		{
			if (!n)
				return nullptr;

			Dir next = Dir::Right;

			if (n->right)
			{
				n = LeftmostOf(n->Nright());
			}
			else
			{
				auto b = n->Branch();

				while (nullptr != (n = n->NSafeParent()))
				{
					if (b == Dir::Left)
						break;
					else
						b = n->Branch();
				};
			}

			return n;
		}
		/*
			Finds the place for given value under this node

			if Dir returned is Dir::None, then returned pointer contains exact match,
			otherwise the pointer is for the last compared node and Dir tells on
			which side of this node given value should be inserted.

		*/
		template <typename Tc>
		std::pair<nptr, Dir> _InsertionPointFor(const Tu& v)
		{
			nptr n = this;

			while (n)
			{
				if (Tc()(n->payload, v))
				{
					if (n->right)
						n = n->Nright();
					else
						return { n, Dir::Right };
				}
				else if (Tc()(v, n->payload))
				{
					if (n->left)
						n = n->Nleft();
					else
						return { n, Dir::Left };
				}
				else
					break;
			}

			return { n, Dir::None };

		}


#pragma region Iterator
		struct iterator
		{
			iterator(nptr n) { n_ = LeftmostOf(n); }
			iterator() {};

			static iterator from_node(nptr n) { iterator it; it.n_ = n; return it; }

			inline operator bool() const { return n_ ? true : false; }

			inline bool operator!=(const iterator& o) { return n_ != o.n_; }

			iterator& operator++() { n_ = InorderNextOf(n_); return *this; }

			const Tu& operator*() { return n_->payload; }


			nptr n_ = { nullptr };

			inline nptr node() const { return n_; }

		};
#pragma endregion

	private:


		/*
			called on a node with both children present, to exchange it with the other node, which is deeper than this one,
			but has 0 or 1 children

		*/
		inline void _SwapWith(nptr o)
		{
			//all these nodes MUST be present
			auto PB = o->Nparent(), AL = Nleft(), AR = Nright();

			if (o == AL)
			{
				//special case swap with one of the child nodes
				//left child can have its own left child, but not the right one
				if (auto PA = NSafeParent())
				{
					((PA->left == -(parent)) ? PA->left : PA->right) = _Off(PA, o);
					o->parent = _Off(o, PA);
				}
				else
				{
					//trivial case, the tree has only three nodes and the current node is root
					o->parent = 0;
				}

				parent = left;

				if (o->left)
				{
					o->Nleft()->parent = _Off(o->Nleft(), this);
					left = _Off(this, o->Nleft());
				}
				else
					left = 0;

				AR->parent = _Off(AR, o);
				o->left = _Off(o, this);
				o->right = _Off(o, AR);

				right = 0;
			}
			else if (o == AR)
			{
				//same as the previous case, but on the right
				if (auto PA = NSafeParent())
				{
					((PA->left == -(parent)) ? PA->left : PA->right) = _Off(PA, o);
					o->parent = _Off(o, PA);
				}
				else
				{
					//trivial case, the tree has only three nodes and the current node is root
					o->parent = 0;
				}

				parent = right;

				if (o->right)
				{
					o->Nright()->parent = _Off(o->Nright(), this);
					right = _Off(this, o->Nright());
				}
				else
					right = 0;

				AL->parent = _Off(AL, o);
				o->right = _Off(o, this);
				o->left = _Off(o, AL);

				left = 0;
			}
			else
			{
				//regular case, two nodes are not related, so their links can be updated in any order

				((PB->left == -(o->parent)) ? PB->left : PB->right) = _Off(PB, this);

				if (auto PA = NSafeParent())
				{
					((PA->left == -(parent)) ? PA->left : PA->right) = _Off(PA, o);
					o->parent = _Off(o, PA);
				}
				else
				{
					o->parent = 0;
				}

				parent = _Off(this, PB);


				//Left
				{
					AL->parent = _Off(AL, o);

					if (auto BL = o->NSafeLeft())
					{
						BL->parent = _Off(BL, this);
						left = _Off(this, BL);
					}
					else
					{
						left = 0;
					}

					o->left = _Off(o, AL);
				}

				//Right
				{
					AR->parent = _Off(AR, o);

					if (auto BR = o->NSafeRight())
					{
						BR->parent = _Off(BR, this);
						right = _Off(this, BR);
					}
					else
					{
						right = 0;
					}

					o->right = _Off(o, AR);
				}
			}

			std::swap(tilt, o->tilt);

		}

		inline static void _Rotate_LL(nptr Z, nptr Y, nptr X)
		{
			//Parent(Z) is wired with Y
			if (auto P = Z->NSafeParent())
			{
				Y->parent += Z->parent;
				((P->left == -(Z->parent)) ? P->left : P->right) += Z->left;
			}
			else
			{
				Y->parent = 0;
			}

			//Y becomes Z's parent
			Z->parent = Z->left;

			//T2 (former right(Y)) is reconnected to Z on the left
			if (Y->right)
			{
				Z->left = Z->left + Y->right;
				Z->Nleft()->parent = -(Z->left);
			}
			else
				Z->left = 0;

			//Right(Y) is repointed to Z
			Y->right = -(Z->parent);
		}
		inline static void _Rotate_RR(nptr Z, nptr Y, nptr X)
		{
			//Parent(Z) is wired with Y
			if (auto P = Z->NSafeParent())
			{
				Y->parent += Z->parent;
				((P->left == -(Z->parent)) ? P->left : P->right) += Z->right;
			}
			else
			{
				Y->parent = 0;
			}

			//Y becomes Z's parent
			Z->parent = Z->right;

			//T2 (former Left(Y)) is reconnected to Z on the right
			if (Y->left)
			{
				Z->right = Z->right + Y->left;
				Z->Nright()->parent = -(Z->right);
			}
			else
				Z->right = 0;

			//Left(Y) is repointed to Z
			Y->left = -(Z->parent);

		}
		inline static void _Rotate_LR(nptr Z, nptr Y, nptr X)
		{
			//Parent(Z) becomes parent(X)
			if (auto P = Z->NSafeParent())
			{
				((P->left == -(Z->parent)) ? P->left : P->right) = _Off(P, X);
				X->parent = _Off(X, P);
			}
			else
			{
				X->parent = 0;
			}

			//Z
			Z->parent = _Off(Z, X);
			if (X->right)
			{
				Z->left = Z->parent + X->right;
				Z->Nleft()->parent = -(Z->left);
			}
			else
				Z->left = 0;

			//Y
			Y->parent = Y->right;
			if (X->left)
			{
				Y->right = Y->right + X->left;
				Y->Nright()->parent = -(Y->right);
			}
			else
				Y->right = 0;

			//X
			X->right = _Off(X, Z);
			X->left = _Off(X, Y);
		}
		inline static void _Rotate_RL(nptr Z, nptr Y, nptr X)
		{
			//Parent(Z) becomes parent(X)
			if (auto P = Z->NSafeParent())
			{
				((P->left == -(Z->parent)) ? P->left : P->right) = _Off(P, X);
				X->parent = _Off(X, P);
			}
			else
			{
				X->parent = 0;
			}

			//Z
			Z->parent = _Off(Z, X);
			if (X->left)
			{
				Z->right = Z->parent + X->left;
				Z->Nright()->parent = -(Z->right);
			}
			else
				Z->right = 0;

			//Y
			Y->parent = Y->left;
			if (X->right)
			{
				Y->left = Y->left + X->right;
				Y->Nleft()->parent = -(Y->left);
			}
			else
				Y->left = 0;

			//X
			X->right = _Off(X, Y);
			X->left = _Off(X, Z);
		}
	}; //Node

#pragma endregion

}

#endif

