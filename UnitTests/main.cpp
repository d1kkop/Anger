#include "UnitTest.h"

#include <cstdlib>
#include <vld.h>


int main(int argc, char** argv)
{
	UnitTests::NetworkTests::RunAll();
	::system("pause");
	return 0;
}