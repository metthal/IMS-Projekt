#include <ctime>
#include <simlib.h>

int main(int argc, char* argv[])
{
    RandomSeed(time(NULL));
    Init(0, 1000);
    Run();
}
