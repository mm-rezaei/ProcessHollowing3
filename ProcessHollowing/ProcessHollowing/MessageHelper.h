#pragma once

class MessageHelper
{
private:

	static void PrintIntroduction(int inMethodNumber)
	{
		std::wcout << std::endl;
		std::wcout << L"Process Hollowing (Method " << inMethodNumber << ")" << std::endl;
	}

	static void PrintMethodSpecification()
	{
		std::wcout << std::endl;
		std::wcout << L"\tMethod Specification:" << std::endl;
		std::wcout << std::endl;
		std::wcout << L"\t\t1- Windows APIs used in general method of Process Hollowing (+ Determine which ones are being used in the current method):" << std::endl;
		std::wcout << std::endl;
		std::wcout << L"\t\t\tUsed:" << std::endl;
		std::wcout << L"\t\t\t\ta. CreateProcessW" << std::endl;
		std::wcout << L"\t\t\t\tb. NtGetContextThread" << std::endl;
		std::wcout << L"\t\t\t\tc. NtReadVirtualMemory" << std::endl;
		std::wcout << L"\t\t\t\td. NtResumeThread" << std::endl;
		std::wcout << L"\t\t\t\tf. NtWriteVirtualMemory" << std::endl;
		std::wcout << L"\t\t\t\tg. VirtualAllocEx" << std::endl;
		std::wcout << L"\t\t\t\ta. VirtualProtectEx" << std::endl;
		std::wcout << std::endl;
		std::wcout << L"\t\t\tNot Used:" << std::endl;
		std::wcout << L"\t\t\t\ta. NtSetContextThread" << std::endl;
		std::wcout << std::endl;
		std::wcout << L"\t\t2- Provide only support following data directories: Import Table, Relocation, TLS, and Import Address Table (Otherwise maybe not working)" << std::endl;
		std::wcout << L"\t\t3- The existence of the relocation directory is mandatory" << std::endl;
		std::wcout << L"\t\t4- Perform process hollowing without altering the host process's details, including its image, context, entry point address, and instructions at the entry point" << std::endl;
	}

	static void PrintGuidance()
	{
		std::wcout << std::endl;
		std::wcout << L"\tUsage Guide:" << std::endl;
		std::wcout << std::endl;
		std::wcout << L"\t\tThis tool only supports x86 (32-bit) PE types" << std::endl;
		std::wcout << L"\t\tYou need to provide two parameters in the following format:" << std::endl;
		std::wcout << std::endl;
		std::wcout << L"\t\t\tProcessHollowing.exe \"filename1\" \"filename2\"" << std::endl;
		std::wcout << std::endl;
		std::wcout << L"\t\t\tfilename1: The full path of the file that you want to inject (RunPE)" << std::endl;
		std::wcout << L"\t\t\tfilename2: The full path of the file that is used as a host process" << std::endl;
		std::wcout << std::endl;
		std::wcout << L"\t\t\tUsage Example: ProcessHollowing.exe \"C:\\path\\to\\inject.exe\" \"C:\\Windows\\SysWOW64\\explorer.exe\"" << std::endl;
	}

	static void PrintFile1CriteriaList()
	{
		std::wcout << std::endl;
		std::wcout << L"\t\t\ta. Must be a valid x86 (32-bit) Portable Executable (PE) format" << std::endl;
		std::wcout << L"\t\t\tb. The existence of the relocation directory is mandatory" << std::endl;
		std::wcout << L"\t\t\tc. The only supported data directories are Import Table, Relocation, TLS, and Import Address Table (It is possible to develop the others)" << std::endl;
	}

	static void PrintFile2CriteriaList()
	{
		std::wcout << std::endl;
		std::wcout << L"\t\t\ta. Must be a valid x86 (32-bit) Portable Executable (PE) format" << std::endl;
	}

	static void PrintInputCriteriaList()
	{
		std::wcout << std::endl;
		std::wcout << L"\tCriteria List of Input Files:" << std::endl;
		std::wcout << std::endl;
		std::wcout << L"\t\tCriteria List of File 1:" << std::endl;

		PrintFile1CriteriaList();

		std::wcout << std::endl;

		std::wcout << L"\t\tCriteria List of File 2:" << std::endl;

		PrintFile2CriteriaList();
	}

public:

	static void PrintHelp(int inMethodNumber, const wchar_t* inFilename1, const wchar_t* inFilename2, bool inFilename1Validation, bool inFilename2Validation)
	{
		PrintIntroduction(inMethodNumber);
		PrintMethodSpecification();
		PrintGuidance();
		PrintInputCriteriaList();

		std::wcout << std::endl;
		std::wcout << L"\tInput Files:" << std::endl;
		std::wcout << std::endl;
		std::wcout << L"\t\tfilename1: " << inFilename1 << std::endl;
		std::wcout << L"\t\tfilename2: " << inFilename2 << std::endl;
		std::wcout << std::endl;
		std::wcout << L"\t\tfilename1 validation: " << (inFilename1Validation ? L"Is supported" : L"Is not supported") << std::endl;
		std::wcout << L"\t\tfilename2 validation: " << (inFilename2Validation ? L"Is supported" : L"Is not supported") << std::endl;

		std::wcout << std::endl;
		std::wcout << L"\tMessages:" << std::endl;
		std::wcout << std::endl;
	}

	static void PrintMessage(const wchar_t* inMessage)
	{
		std::wcout << L"\t\t" << inMessage << std::endl;
	}
};

