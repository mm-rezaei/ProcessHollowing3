
# Process Hollowing (Method 3 of 3)

## Overview

**Process Hollowing** is a sophisticated code injection technique often used by malware to disguise its malicious activities. In this method, an attacker creates a legitimate process in a suspended state, replaces the process’s code with malicious code, and then resumes the execution of the now hollowed process. This technique allows the malicious code to run under the guise of a legitimate process, making it difficult for security solutions to detect the malicious activity.

**Disclaimer:** This code is developed solely for educational purposes. It is intended to demonstrate how Process Hollowing works to enhance understanding and improve defensive measures. **Do not use this code for malicious activities.**

### Other Names

Process Hollowing is also known by several other names:
- **RunPE**
- **Process Replacement**
- **PE Injection**

## Purpose and Use Cases

Process Hollowing is primarily used by malware developers to:
- Evade detection by antivirus and security software.
- Run malicious code with the privileges of a legitimate process.
- Maintain persistence on an infected system.
- Avoid triggering suspicious behavior alerts by blending with normal system processes.

## Steps in Process Hollowing: A General Approach

Process Hollowing involves several steps:
1. **Creation of a Suspended Process**: The attacker begins by creating a new process in a suspended state. This is usually a legitimate process to avoid raising any suspicions.
2. **Injecting Malicious Code**: The attacker injects their malicious code into the address space of the suspended process.
3. **Adjusting the Entry Point**: The entry point of the process is adjusted to point to the malicious code.
4. **Resuming the Process**: The now-hollowed process is resumed, allowing the malicious code to execute under the guise of the original legitimate process.

In the general approach to process hollowing, several Windows APIs are utilized, including:

	- CreateProcessW
	- NtGetContextThread
	- NtReadVirtualMemory
	- NtResumeThread
	- NtSetContextThread
	- NtWriteVirtualMemory
	- VirtualAllocEx
	- VirtualProtectEx

The simplest way for anti-malware solutions to detect process hollowing is often by tracking the sequences of these API calls. Another method of detection involves analyzing the memory of running processes to identify inconsistencies or the presence of injected code. However, I have developed three customized methods to reduce the likelihood of detection by anti-malware products. Additionally, I employed a custom range of the above Windows APIs, which may present further challenges for detection by such products.

## Summary of 3 Developed Methods

1. **Method 1:** This method is a general technique for process hollowing and supports all data directories. The presence of the relocation directory is mandatory. In this method, the host image base is modified and the host process context is updated using NtSetContextThread. These two modifications can make the process more detectable by anti-malware software, increasing the likelihood of detection ([ProcessHollowing - Method 1](https://github.com/mm-rezaei/ProcessHollowing1)).

2. **Method 2:** This method differs from Method 1 in that it makes the process more elegant by not using two Windows APIs, at least one of which is crucial for making it undetectable. These two Windows APIs are NtReadVirtualMemory and NtSetContextThread. Additionally, this method supports all data directories, with the relocation directory being mandatory. While the host image base is modified in this method, the host process context is not updated using NtSetContextThread. This approach can make the process less detectable by anti-malware software ([ProcessHollowing - Method 2](https://github.com/mm-rezaei/ProcessHollowing2)).

3. **Method 3:** This method differs from the two previous methods in that it incorporates significant changes, making the process more specialized and providing advantages that make it harder to detect. Like Method 2, this approach does not use NtSetContextThread to update the host process context. It performs process hollowing without altering the host process's details, such as its image, context, entry point address, and instructions at the entry point. These modifications enhance the method's effectiveness. This method can support all data directories, but currently, I have only developed support for the following data directories, Import Table, Relocation, TLS, and Import Address Table. Note that the Relocation data directory is mandatory. If the PE file you wish to inject contains data directories that are not supported, the method may not work as intended. If you need to support additional data directories, you can extend the development to include them ([ProcessHollowing - Method 3](https://github.com/mm-rezaei/ProcessHollowing3)).

## Current Method Specification

1. This tool only supports x86 (32-bit) PE types
2. Windows APIs used in general method of Process Hollowing (+ Determine which ones are being used in the current method):

   2.1. **List of Used Windows APIs in this Method:**
   - `CreateProcessW`
   - `NtGetContextThread`
   - `NtReadVirtualMemory`
   - `NtResumeThread`
   - `NtWriteVirtualMemory`
   - `VirtualAllocEx`
   - `VirtualProtectEx`

   2.2. **List of Windows APIs Not Utilized in this Method:**
   - `NtSetContextThread`

3. Provide only support following data directories: Import Table, Relocation, TLS, and Import Address Table (Otherwise maybe not working)
4. The existence of the relocation directory is mandatory
5. Perform process hollowing without altering the host process's details, including its image, context, entry point address, and instructions at the entry point

## Usage Guide
		
You need to provide two parameters in the following format:

	ProcessHollowing.exe "filename1" "filename2"

	filename1: The full path of the file that you want to inject (RunPE)
	filename2: The full path of the file that is used as a host process

	Usage Example: ProcessHollowing.exe "C:\path\to\inject.exe" "C:\Windows\SysWOW64\explorer.exe"

## Input File Criterias

Criteria List of File 1:

	a. Must be a valid x86 (32-bit) Portable Executable (PE) format
	b. The existence of the relocation directory is mandatory

Criteria List of File 2:

	a. Must be a valid x86 (32-bit) Portable Executable (PE) format

## Detection Methods

Detecting Process Hollowing can be challenging due to its stealthy nature. However, some strategies can help in its identification:
- **Behavioral Analysis**: Monitoring for unusual behavior patterns such as legitimate processes performing unexpected actions.
- **Memory Forensics**: Analyzing the memory of running processes to detect inconsistencies or the presence of injected code.
- **Endpoint Detection and Response (EDR)**: Employing advanced EDR solutions that can detect and respond to suspicious activities in real-time.

Mitigation strategies include:
- **Code Signing**: Ensuring that only signed and verified code can execute on the system.
- **Behavioral Monitoring**: Continuous monitoring for abnormal process behaviors and activities.
- **Least Privilege Principle**: Running processes with the least privileges necessary to limit the potential damage of a successful attack.

## Conclusion

Process Hollowing is a powerful technique used by attackers to execute malicious code covertly. By understanding its mechanism and implementing robust detection and mitigation strategies, it is possible to defend against such sophisticated threats and protect systems from being compromised.

## Important Notice

This project is intended for educational purposes only. The techniques demonstrated here are commonly used by malware, and it is important to understand them to develop effective defenses. **Do not use this code for malicious activities.**