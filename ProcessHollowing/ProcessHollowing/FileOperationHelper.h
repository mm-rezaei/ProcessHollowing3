#pragma once

class FileOperationHelper
{
public:

	static byte* ReadFileContent(const wchar_t* filename, int* outLength)
	{
		byte* result = nullptr;

		*outLength = 0;

		HANDLE hFile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hFile != INVALID_HANDLE_VALUE)
		{
			LARGE_INTEGER fileSize;

			if (GetFileSizeEx(hFile, &fileSize))
			{
				*outLength = static_cast<int>(fileSize.QuadPart);

				result = new byte[*outLength];

				DWORD bytesRead;

				if (!ReadFile(hFile, result, *outLength, &bytesRead, NULL) || bytesRead != *outLength)
				{
					delete[] result;

					result = nullptr;
				}
			}

			CloseHandle(hFile);
		}

		return result;
	}
};

