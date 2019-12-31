#include <iostream>
#include "core/iavl.h"



/*
	Unit test for indexed::set object

*/
#include <set>
#include <vector>
#include <chrono>
#include <inttypes.h>

using u32 = typename uint32_t;


/*
	Used for insertion/deletion, 
	x - carries the value, y is used for resorting
*/
struct uint2
{
	u32 x, y;

	inline bool operator<(const uint2& o) const { return x < o.x; }
	inline bool operator!=(const uint2& o) const { return x != o.x; }
};

//forwards


void Scramble(std::vector<uint2>& list);


int main()
{

	size_t LEN = 256 * 1024 - 1;

	std::vector<uint2> Src;
	Src.reserve(LEN);

	for (u32 i = 0; i < LEN; i++)
	{
		Src.push_back({ i,0 });
	}



	std::set<uint2>		Set;
	indexed::set<uint2> ISet;

	ISet.reserve(Src.size());

	//start with source array in ASC sort order

	float setMs = 0, iSetMs = 0;

	//Set, insert ASC
	{
		auto t0 = std::chrono::high_resolution_clock::now();
		for (const auto& v : Src)
		{
			Set.insert(v);
		}
		setMs = (std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - t0)).count();
	}

	//ISet, insert ASC
	{
		auto t0 = std::chrono::high_resolution_clock::now();
		for (const auto& v : Src)
		{
			ISet.insert(v);
		}
		iSetMs = (std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - t0)).count();
	}

	std::cout << "ASC insertion " << std::endl;
	std::cout << "  Set:\t" << setMs << "(ms)" << ", size: " << Set.size() << std::endl;
	std::cout << " ISet:\t" << iSetMs << "(ms)" <<", size: " << ISet.size() << std::endl;


	std::cout << std::endl << "======================ISet for sorted insert===============================" << std::endl;
	ISet.dbg_report();
	std::cout << std::endl;
		

	/*
		Source array is resorted in DESC order, insert operations will not add any new elements
	*/
	std::sort(Src.begin(), Src.end(), [](const uint2& a, const uint2& b) { return b.x < a.x; });

	
	Scramble(Src);

	//Set, scrambled erase
	{
		auto t0 = std::chrono::high_resolution_clock::now();
		for (const auto& v : Src)
		{
			Set.erase(v);
		}
		setMs = (std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - t0)).count();
	}

	//ISet, scrambled erase
	{
		auto t0 = std::chrono::high_resolution_clock::now();
		for (const auto& v : Src)
		{
			ISet.erase
			(v);
		}
		iSetMs = (std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - t0)).count();
	}
	std::cout << "Scrambled erase" << std::endl;
	std::cout << "  Set:\t" << setMs << "(ms)" << ", size: " << Set.size() << std::endl;
	std::cout << " ISet:\t" << iSetMs << "(ms)" << ", size: " << ISet.size() << std::endl;



	

	//Set, scrambled insert
	{
		auto t0 = std::chrono::high_resolution_clock::now();
		for (const auto& v : Src)
		{
			Set.insert(v);
		}
		setMs = (std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - t0)).count();
	}

	//ISet, scrambled insert
	{
		auto t0 = std::chrono::high_resolution_clock::now();
		for (const auto& v : Src)
		{
			ISet.insert(v);
		}
		iSetMs = (std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - t0)).count();
	}

	std::cout << "Scrambled insertion " << std::endl;
	std::cout << "  Set:\t" << setMs << "(ms)" << ", size: " << Set.size() << std::endl;
	std::cout << " ISet:\t" << iSetMs << "(ms)" << ", size: " << ISet.size() << std::endl;


	std::cout << std::endl << "======================ISet for random insert===============================" << std::endl;
	ISet.dbg_report();
	std::cout << std::endl;


	//final comparison
	auto a1 = Set.begin(), b1 = Set.end();
	auto a2 = ISet.begin(), b2 = ISet.end();

	bool failed = false;
	for (; a1 != b1; ++a1, ++a2)
	{
		if (*a1 != *a2)
		{
			failed = true;
			break;
		}
	}

	if (failed)
	{
		std::cout << "ERROR: order of items is not the same" << std::endl;
	}
	else
	{
		std::cout << "Order of items is verified" << std::endl;
	}
}


/*
	uint2::y is filled with randeom values and entire list is sorted on it
*/
void Scramble(std::vector<uint2>& list)
{
	for (auto& v : list)
	{
		v.y = (((u32)rand()) << 16) | (((u32)rand()) & 0xffff);
	}

	std::sort(list.begin(), list.end(), [](const uint2& a, const uint2& b) { return a.y < b.y; });
}


