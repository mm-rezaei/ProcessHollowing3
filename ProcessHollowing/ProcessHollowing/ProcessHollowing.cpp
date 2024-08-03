#include <iostream>
#include <windows.h>
#include <string>

#include "MessageHelper.h"
#include "WindowsApiHelper.h"
#include "FileOperationHelper.h"
#include "PortableExecutableParser.h"
#include "ProcessHollowingHelper.h"

using namespace std;

bool IsSupportedAsHostFile(wchar_t* filename)
{
	return ProcessHollowingHelper::IsSupportedAsHostFile(filename);
}

bool IsSupportedAsInjectedFile(wchar_t* filename, bool* outIsDataDirectoriesSupported)
{
	auto result = false;

	auto length = 0;

	auto fileContent = FileOperationHelper::ReadFileContent(filename, &length);

	if (length != 0 && fileContent != nullptr)
	{
		result = ProcessHollowingHelper::IsSupportedAsInjectedFile(fileContent, outIsDataDirectoriesSupported);

		delete fileContent;
	}

	return result;
}

bool Inject(wchar_t* filename, wchar_t* hostFilename)
{
	auto result = false;

	auto length = 0;

	auto fileContent = FileOperationHelper::ReadFileContent(filename, &length);

	if (length != 0 && fileContent != nullptr)
	{
		HANDLE outProcessHandle = NULL;

		result = ProcessHollowingHelper::Run(fileContent, hostFilename, &outProcessHandle);
	}

	return result;
}

int main()
{
	//
	auto cmdLine = GetCommandLineW();

	int argc;

	auto argv = CommandLineToArgvW(cmdLine, &argc);

	//
	InitializeNtFunctions();

	if (argv != nullptr && argc > 2)
	{
		//
		bool isDataDirectoriesSupported;

		auto isSupported1 = IsSupportedAsInjectedFile(argv[1], &isDataDirectoriesSupported);
		auto isSupported2 = IsSupportedAsHostFile(argv[2]);

		MessageHelper::PrintHelp(3, argv[1], argv[2], isSupported1, isSupported2);

		if (isSupported1 && isSupported2)
		{
			wstring message;

			if (Inject(argv[1], argv[2]))
			{
				if (!isDataDirectoriesSupported)
				{
					wstring dataDirectoriesMessage = L"\tWarnning: '" + wstring(argv[1]) + L"' contains unsupported data directories, which may cause it to malfunction!";

					MessageHelper::PrintMessage(dataDirectoriesMessage.c_str());
				}

				message = L"'" + wstring(argv[1]) + L"' was injected to '" + wstring(argv[2]) + L"' successfully!";
			}
			else
			{
				message = L"The injection process was failed!";
			}

			MessageHelper::PrintMessage(message.c_str());
		}
		else
		{
			MessageHelper::PrintMessage(L"The injection process was failed due to the input files!");
		}
	}
	else
	{
		MessageHelper::PrintHelp(3, L"unknown!", L"unknown!", false, false);
		MessageHelper::PrintMessage(L"Please read the usage guide, as the usage was incorrect!");
	}

	std::wcout << std::endl;

	//
	LocalFree(argv);

	return 0;
}
