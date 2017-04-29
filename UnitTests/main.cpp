#include "UnitTest.h"

#include <cstdlib>


int main(int argc, char** argv)
{
	UnitTests::NetworkTests::RunAll();
	::system("pause");
	return 0;
}