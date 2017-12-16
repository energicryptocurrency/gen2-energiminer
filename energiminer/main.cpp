/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file main.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 * Ethereum client.
 */

#include "MinerAux.h"
#include "BuildInfo.h"
#include "energiminer/common.h"

#include <thread>
#include <fstream>
#include <iostream>

using namespace std;

namespace energi
{
  extern int g_logVerbosity;
}

void help()
{
	cout
		<< "Usage ethminer [OPTIONS]" << endl
		<< "Options:" << endl << endl;
	MinerCLI::streamHelp(cout);
	cout
		<< "General Options:" << endl
		<< "    -v,--verbosity <0 - 9>  Set the log verbosity from 0 to 9 (default: 8)." << endl
		<< "    -V,--version  Show the version and exit." << endl
		<< "    -h,--help  Show this help message and exit." << endl
		;
	exit(0);
}

void version()
{
	cout << "enegiminer version " << ENERGI_PROJECT_VERSION << endl;
	cout << "Build: " << ENERGI_BUILD_PLATFORM << "/" << ENERGI_BUILD_TYPE << endl;
	exit(0);
}

// Currently tested for little endian systems
int is_little_endian() { return 0x1 == *reinterpret_cast<const uint16_t*>("\1\0"); }

int main(int argc, char** argv)
{
	if ( !is_little_endian() )
	{
		cerr << "Little endian not tested" << endl;
		exit(-1);
	}

	// Set env vars controlling GPU driver behavior.
	energi::setenv("GPU_MAX_HEAP_SIZE", "100");
	energi::setenv("GPU_MAX_ALLOC_PERCENT", "100");
	energi::setenv("GPU_SINGLE_ALLOC_PERCENT", "100");

	MinerCLI m(MinerCLI::OperationMode::GBT);

	for (int i = 1; i < argc; ++i)
	{
		// Mining options:
		if (m.interpretOption(i, argc, argv))
			continue;

		// Standard options:
		string arg = argv[i];
		if ((arg == "-v" || arg == "--verbosity") && i + 1 < argc)
			energi::g_logVerbosity = atoi(argv[++i]);
		else if (arg == "-h" || arg == "--help")
			help();
		else if (arg == "-V" || arg == "--version")
			version();
		else
		{
			cerr << "Invalid argument: " << arg << endl;
			exit(-1);
		}
	}

	m.execute();

	return 0;
}
