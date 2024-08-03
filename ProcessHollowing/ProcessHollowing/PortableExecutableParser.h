#pragma once

class PortableExecutableParser
{
private:

	BYTE* _PeContent;

public:

	explicit PortableExecutableParser(BYTE* inPeContent)
	{
		_PeContent = inPeContent;
	}

	__declspec(property(get = GetImageDosHeader)) PIMAGE_DOS_HEADER DosHeader;

	__declspec(property(get = GetImageNtHeader)) PIMAGE_NT_HEADERS NtHeader;

	__declspec(property(get = GetPeContent)) BYTE* PeContent;

	__declspec(property(get = GetHasDosSignature)) bool HasDosSignature;

	__declspec(property(get = GetHasNtSignature)) bool HasNtSignature;

	__declspec(property(get = GetImageBase)) DWORD ImageBase;

	__declspec(property(get = GetSectionCount)) WORD SectionCount;

	__declspec(property(get = GetSizeOfImage)) DWORD SizeOfImage;

	__declspec(property(get = GetSizeOfOptionalHeader)) WORD SizeOfOptionalHeader;

	enum DirectoryType
	{
		NoneDirectory = -1,
		ExportDirectory = 0,
		ImportDirectory = 1,
		ResourceDirectory = 2,
		ExceptionDirectory = 3,
		SecurityDirectory = 4,
		RelocationDirectory = 5,
		DebugDirectory = 6,
		ArchitectureDirectory = 7,
		ReservedDirectory = 8,
		TlsDirectory = 9,
		ConfigurationDirectory = 10,
		BoundImportDirectory = 11,
		ImportAddressTableDirectory = 12,
		DelayImportDirectory = 13,
		DotNetMetaDataDirectory = 14
	};

	PIMAGE_DOS_HEADER GetImageDosHeader()
	{
		return PIMAGE_DOS_HEADER(PeContent);
	}

	PIMAGE_NT_HEADERS GetImageNtHeader()
	{
		return PIMAGE_NT_HEADERS(PeContent + GetImageDosHeader()->e_lfanew);
	}

	BYTE* GetPeContent()
	{
		return _PeContent;
	}

	bool GetHasDosSignature()
	{
		return DosHeader->e_magic == IMAGE_DOS_SIGNATURE;
	}

	bool GetHasNtSignature()
	{
		return NtHeader->Signature == IMAGE_NT_SIGNATURE;
	}

	DWORD GetImageBase()
	{
		return NtHeader->OptionalHeader.ImageBase;
	}

	WORD GetSectionCount()
	{
		return NtHeader->FileHeader.NumberOfSections;
	}

	DWORD GetSizeOfImage()
	{
		return NtHeader->OptionalHeader.SizeOfImage;
	}

	WORD GetSizeOfOptionalHeader()
	{
		return NtHeader->FileHeader.SizeOfOptionalHeader;
	}

	PIMAGE_SECTION_HEADER GetSectionHeader(DWORD inSectionNumber)
	{
		return PIMAGE_SECTION_HEADER(PeContent + DosHeader->e_lfanew + 4 + 20 + SizeOfOptionalHeader + inSectionNumber * 40);
	}

	DWORD GetFileOffsetOfRawDataOfSection(DWORD inSectionNumber)
	{
		return GetSectionHeader(inSectionNumber)->PointerToRawData;
	}

	BYTE* GetAbsolutePointerOfRawDataOfSection(DWORD inSectionNumber)
	{
		return PeContent + GetFileOffsetOfRawDataOfSection(inSectionNumber);
	}

	DWORD GetRelativeVirtualAddressOfSection(DWORD inSectionNumber)
	{
		return GetSectionHeader(inSectionNumber)->VirtualAddress;
	}

	DWORD GetVirtualAddressOfSection(DWORD inSectionNumber)
	{
		return ImageBase + GetRelativeVirtualAddressOfSection(inSectionNumber);
	}

	bool IsDataDirectoryEmpty(DWORD inDirectoryNumber)
	{
		auto result = false;

		auto directory = GetDataDirectory(inDirectoryNumber);

		if (directory->VirtualAddress + directory->Size == 0)
		{
			result = true;
		}

		return result;
	}

	bool IsDataDirectoryEmpty(DirectoryType inDirectoryType)
	{
		return IsDataDirectoryEmpty(static_cast<DWORD>(inDirectoryType));
	}

	PIMAGE_DATA_DIRECTORY GetDataDirectory(DWORD inDirectoryNumber)
	{
		PIMAGE_DATA_DIRECTORY result = reinterpret_cast<PIMAGE_DATA_DIRECTORY>(reinterpret_cast<DWORD>(PeContent) + DosHeader->e_lfanew + 0x78 + inDirectoryNumber * sizeof(IMAGE_DATA_DIRECTORY));

		return result;
	}

	PIMAGE_DATA_DIRECTORY GetDataDirectory(DirectoryType inDirectoryType)
	{
		return GetDataDirectory(static_cast<DWORD>(inDirectoryType));
	}

	void SetDataDirectory(DWORD inDirectoryNumber, DWORD inVirtualAddress, DWORD inSize)
	{
		PIMAGE_DATA_DIRECTORY dataDirectory = reinterpret_cast<PIMAGE_DATA_DIRECTORY>(reinterpret_cast<DWORD>(PeContent) + DosHeader->e_lfanew + 0x78 + inDirectoryNumber * sizeof(IMAGE_DATA_DIRECTORY));

		dataDirectory->VirtualAddress = inVirtualAddress;
		dataDirectory->Size = inSize;
	}

	void SetDataDirectory(DirectoryType inDirectoryType, DWORD inVirtualAddress, DWORD inSize)
	{
		SetDataDirectory(static_cast<DWORD>(inDirectoryType), inVirtualAddress, inSize);
	}

	void SetDataDirectoryEmpty(DWORD inDirectoryNumber)
	{
		SetDataDirectory(inDirectoryNumber, 0, 0);
	}

	void SetDataDirectoryEmpty(DirectoryType inDirectoryType)
	{
		SetDataDirectoryEmpty(static_cast<DWORD>(inDirectoryType));
	}

	DirectoryType GetRelatedDirectoryOfRelativeVirtualAddress(DWORD inAddress)
	{
		auto result = NoneDirectory;

		for (DWORD index = 0; index <= DotNetMetaDataDirectory; index++)
		{
			auto directory = GetDataDirectory(index);

			if (directory->VirtualAddress + directory->Size != 0)
			{
				if (directory->VirtualAddress <= inAddress && inAddress < directory->VirtualAddress + directory->Size)
				{
					result = static_cast<DirectoryType>(index);

					break;
				}
			}
		}

		return result;
	}

	DirectoryType GetRelatedDirectoryOfVirtualAddress(DWORD inAddress)
	{
		return GetRelatedDirectoryOfRelativeVirtualAddress(inAddress - ImageBase);
	}

	DWORD GetRelatedSectionNumberOfRelativeVirtualAddress(DWORD inAddress)
	{
		return GetRelatedSectionNumberOfVirtualAddress(inAddress + ImageBase);
	}

	DWORD GetRelatedSectionNumberOfVirtualAddress(DWORD inAddress)
	{
		DWORD result = -1;

		for (int index = SectionCount - 1; index >= 0; index--)
		{
			PIMAGE_SECTION_HEADER sectionHeader = GetSectionHeader(index);

			if (sectionHeader->VirtualAddress + ImageBase <= inAddress)
			{
				result = index;

				break;
			}
		}

		return result;
	}

	DWORD GetRelatedSectionNumberOfFileOffset(DWORD inFileOffset)
	{
		DWORD result = -1;

		for (int index = SectionCount - 1; index >= 0; index--)
		{
			PIMAGE_SECTION_HEADER sectionHeader = GetSectionHeader(index);

			if (sectionHeader->PointerToRawData <= inFileOffset)
			{
				result = index;

				break;
			}
		}

		return result;
	}

	DWORD GetFileOffsetOfRelativeVirtualAddress(DWORD inRelativeVirtualAddress)
	{
		return GetFileOffsetOfVirtualAddress(inRelativeVirtualAddress + ImageBase);
	}

	DWORD GetFileOffsetOfVirtualAddress(DWORD inVirtualAddress)
	{
		DWORD result;

		DWORD sectionNumber = GetRelatedSectionNumberOfVirtualAddress(inVirtualAddress);

		DWORD fileOffsetOfRawDataOfSection = GetFileOffsetOfRawDataOfSection(sectionNumber);

		result = fileOffsetOfRawDataOfSection + (inVirtualAddress - GetSectionHeader(sectionNumber)->VirtualAddress - ImageBase);

		return result;
	}

	DWORD GetVirtualAddressOfFileOffset(DWORD inFileOffset)
	{
		return GetRelativeVirtualAddressOfFileOffset(inFileOffset) + ImageBase;
	}

	DWORD GetVirtualAddressOfRelativeVirtualAddress(DWORD inRelativeVirtualAddress)
	{
		return inRelativeVirtualAddress + ImageBase;
	}

	DWORD GetRelativeVirtualAddressOfFileOffset(DWORD inFileOffset)
	{
		DWORD result;

		DWORD sectionNumber = GetRelatedSectionNumberOfFileOffset(inFileOffset);

		DWORD relativeVirtualAddressOfSection = GetRelativeVirtualAddressOfSection(sectionNumber);

		result = relativeVirtualAddressOfSection + (inFileOffset - GetSectionHeader(sectionNumber)->PointerToRawData);

		return result;
	}

	DWORD GetRelativeVirtualAddressOfVirtualAddress(DWORD inVirtualAddress)
	{
		return inVirtualAddress - ImageBase;
	}

	bool AreBothOfVirtualAddressesInTheSameSection(DWORD inVirtualAddress1, DWORD inVirtualAddress2)
	{
		return GetRelatedSectionNumberOfVirtualAddress(inVirtualAddress1) == GetRelatedSectionNumberOfVirtualAddress(inVirtualAddress2);
	}

	bool AreBothOfRelativeVirtualAddressesInTheSameSection(DWORD inRelativeVirtualAddress1, DWORD inRelativeVirtualAddress2)
	{
		return GetRelatedSectionNumberOfRelativeVirtualAddress(inRelativeVirtualAddress1) == GetRelatedSectionNumberOfRelativeVirtualAddress(inRelativeVirtualAddress2);
	}
};
