# indexed_set

A container that has properties of both std::vector and std::set, internally organized as an AVL tree, where inserted values
can be addresssed by value (as in std::set) or by by integer index, or slot value (as in std::vector).

Elements are stored in one contiguous memory segment and can be easily serialized/deserialized/cloned in a single byte-copy
operation.

'Erased' tree nodes are marked as 'deleted' and can be reused later, so any slot number assigned to any particular inserted value 
is valid until the moment when it is explicitly erased. Desctructor is called when the value is erased, and the node that carries it is 
wiped and placed in 'deleted' queue. 

So, in this case, this object deviates from behaviour of std::vecrtor, which does not allow 
for 'holes' left in occupied memory.
However, this container supports 'reserve', unlike original std::set, when the number of elements is known upfront of can be 'lucky-guessed'.


I am using it to store big (or huge, dependging on how you look at it) arrays of 3d points and other POD types, where individual
allocation/deallocation can be painful or not desired.


Underlying type T should be trivially copyable, because memcpy is used when the array needs to grow.


