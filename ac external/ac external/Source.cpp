#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <tchar.h>
#include <vector>



//Change these based on desired process to scan
static const wchar_t* FILE_NAME = L"ac_client.exe";
static const wchar_t* MODULE_NAME = L"ac_client.exe";

//Offsets I found in Cheat Engine for AC version 1.3.0.2
static const uintptr_t LOCAL_PLAYER_OFFSET = 0x18AC00;
static const std::vector<unsigned int> HEALTH_OFFSET = { 0xEC };
//Vector bc might be multilevel pointers and I didn't want to create an array w a max buffer size
//This is more dynamic


template <typename X>

bool overWriteValue(HANDLE hProc, X newValue, BYTE* ptr);
//Forward dec for overWriteValue --> takes template type bc might be float or double or int

DWORD gProcessID(const wchar_t* fName) {
	//Takes a const fName (unicode bc windows does that) and iterates thru processes until strcmp matches

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	//Taking a snapshot of all processes
	if (snapshot == INVALID_HANDLE_VALUE) exit(0);
	//Error check

	PROCESSENTRY32 pe32{};		//process entry iterator
	pe32.dwSize = sizeof(pe32);	//Req (read msdn for more info) *to ensure that they r the same size
	Process32First(snapshot, &pe32); //Sets process entry to the first process

	while (_wcsicmp(pe32.szExeFile, fName)) //loops until string compare returns 0 (if they rnt different)
	{
		if (!Process32Next(snapshot, &pe32)) //Error checking if it runs out of processes
		{
			std::cout << "[*] ERROR FINDING PID; NO MORE PROCESSES \n";
			exit(0);
		}

	}

	if (snapshot != 0) {
		CloseHandle(snapshot);	//Closes handle to prevent mem leaks
	}
	return pe32.th32ProcessID;	//Returns the processid of the process that passed the strcmp


}

uintptr_t gModuleBaseAddress(DWORD processID, const wchar_t* moduleName) {
	// Literally the same as the previous function except looks for module instead of process
	//Takes procID so only scanning the desired process modules not everything running
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processID);

	if (snapshot == INVALID_HANDLE_VALUE) exit(0);

	MODULEENTRY32 me32{};
	me32.dwSize = sizeof(me32);
	Module32First(snapshot, &me32);

	while (_wcsicmp(me32.szModule, moduleName))
	{

		if (!Module32Next(snapshot, &me32))
		{
			std::cout << "[*] ERROR FINDING ModBaseAddy; NO MORE MODULES \n";
			exit(0);
		}
	}
	if (snapshot != 0) {
		CloseHandle(snapshot);
		return uintptr_t(me32.modBaseAddr);
	}
	return 0;
}


//THIS FUNCTION IS A NICE OFFSET ADDER/FINDER FROM GH THAT I LIKE:
//credit to guidedhacking.com:
uintptr_t FindDMAAddy(HANDLE hProc, uintptr_t ptr, std::vector<unsigned int> offsets) {
	//Reads memory and adds all the offsets to a given pointer --> helpful for mutilevel pointers (like ammo)
	uintptr_t addr = ptr;
	for (int i = 0; i < offsets.size(); i++) {
		ReadProcessMemory(hProc, (BYTE*)addr, &addr, sizeof(addr), 0);
		addr += offsets[i];
	}
	return addr;
}


template<typename T, typename U, typename V, typename W> void print(T procID, U modAddy, V health, W healthValue) {
	//I know this is awful practice but I needed to clean the code up a bit :P
	//Just prints some stuff to console

	std::cout << "[*] Retrieving Process ID...\n";
	std::cout << "\tAssault Cube PID: " << std::hex << procID << std::endl << std::endl;
	std::cout << "[*] Retrieving Module Base Address...\n";
	std::cout << "\tModule Base Address: 0x" << modAddy << std::endl << std::endl;
	std::cout << "[*] Generating Health Information...\n";
	std::cout << "\tThe dynamic health address is: 0x" << std::hex << health << '\n';
	std::cout << "\tCurrent health value: " << std::dec << healthValue << '\n';


}



//End of boiler plate code
//----------------------------------------------------------------------------------------------------------------------------


int payload() {
	//Example payload using above functions


	DWORD procID = gProcessID(FILE_NAME);
	uintptr_t modAddy = gModuleBaseAddress(procID, MODULE_NAME);

	//Creates a handle to the process with all access (wr etc) using the procID
	HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, 0, procID);

	if (!hProc) {	//Ensure that we aren't opening a handle to a null process
		std::cout << "[*] ERROR: There was an error getting a handle to the process\n";
		return -1;
	}

	uintptr_t dPlayerEnt = modAddy + LOCAL_PLAYER_OFFSET;	//Static Player Address (adds offset to base address)
	uintptr_t health = FindDMAAddy(hProc, dPlayerEnt, HEALTH_OFFSET);
	//^Runs DMAAddy with const offset found in cheat engine --> finds multilevel pointer


	//Assign a buffer to read into
	int healthValue = 0;
	ReadProcessMemory(hProc, (BYTE*)health, &healthValue, sizeof(healthValue), nullptr);
	//Overwrites buffer with value at *health
	print(procID, modAddy, health, healthValue);
	//Calls print function to print it out to console

	healthValue = 100000;
	//Buffer becomes value we are writing into *health 
	WriteProcessMemory(hProc, (BYTE*)health, &healthValue, sizeof(healthValue), nullptr);
	//Overwrites *health with buffer healthValue


	print(procID, modAddy, health, healthValue);

	overWriteValue(hProc, 1337, (BYTE*)health);
	//Function I wrote to simplify read/writef --> less bp code
	CloseHandle(hProc);	//prevents memory leaks

	wchar_t t = getchar(); //just to make the console harder to close (must hit enter to close it)

	return 0;
}




template <typename X> bool overWriteValue(HANDLE hProc, X newValue, BYTE* ptr) {
	//Function that simplifies RPM/WPM and prints the original and new value

	int temp = 0;	//temp buffer
	ReadProcessMemory(hProc, ptr, &temp, sizeof(temp), nullptr);
	std::cout << "Current Value is: " << temp << '\n';
	WriteProcessMemory(hProc, ptr, &newValue, sizeof(newValue), nullptr);
	std::cout << "New Value is: " << newValue << '\n';
	return true;
}

int main() {
	payload();
	return 0;
}