/*
 * Machine.cpp
 *
 *  Created on: 11/lug/2013
 *      Author: stefano
 */

// TODO: Reorganize code.

#include "Machine.h"
#include "ProcReader.h"
#include "Support.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>

#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <sstream>
#include <vector>

const char* kBIOSInfo = "BIOS Information";
const char* kSystemInfo = "System Information";
const char* kProcessorInfo = "Processor Info";

Machine::Machine()
	:
	fNumCPUs(0)
{
}


Machine::~Machine()
{
}


void
Machine::RetrieveData()
{
	if (!_GetDMIDecodeData()) {
		std::cerr << "Can't find dmidecode. Is it installed?" << std::endl;
		_GetLSHWData();
	}

	_GetCPUInfo();

	try {
		_GetOSInfo();
	} catch (...) {
		std::cerr << "Failed to get kernel info." << std::endl;
	}
}


std::string
Machine::AssetTag() const
{
	return "";
}


std::string
Machine::BIOSVersion() const
{
	return _GetValue("Version", kBIOSInfo);
}


std::string
Machine::BIOSManufacturer() const
{
	return _GetValue("Vendor", kBIOSInfo);
}


std::string
Machine::BIOSDate() const
{
	return _GetValue("Release Date", kBIOSInfo);
}


std::string
Machine::SystemManufacturer() const
{
	return _GetValue("Manufacturer", kSystemInfo);

}


std::string
Machine::HostName() const
{
	popen_streambuf p("hostname -s", "r");
	std::istream stream(&p);
	std::string line;
	std::getline(stream, line);

	return line;
}


std::string
Machine::SystemModel() const
{
	return _GetValue("Product Name", kSystemInfo);
}


std::string
Machine::SystemSerialNumber() const
{
	return _GetValue("Serial Number", kSystemInfo);
}


std::string
Machine::SystemUUID() const
{
	return _GetValue("UUID", kSystemInfo);
}


std::string
Machine::MachineSerialNumber() const
{
	return _GetValue("Serial Number", "Base Board Information");
}


std::string
Machine::MachineManufacturer() const
{
	return _GetValue("Manufacturer", "Base Board Information");
}


int
Machine::CountProcessors() const
{
	return fNumCPUs;
}


std::string
Machine::ProcessorManufacturer(int numCpu) const
{
	return _ProcessorInfo("vendor_id", numCpu);
}


std::string
Machine::ProcessorSpeed(int numCpu) const
{
	std::string mhz = _ProcessorInfo("cpu MHz", numCpu);

	size_t pos = mhz.find(".");
	if (pos != std::string::npos) {
		mhz = mhz.substr(0, pos);
	}

	return mhz;
}


std::string
Machine::ProcessorSerialNumber(int numCpu) const
{
	//std::string mhz = _GetValue("Serial Number", numCpu);
	return "";
}


std::string
Machine::ProcessorType(int numCpu) const
{
	std::string model = _ProcessorInfo("model name", numCpu);
	trim(model);
	return model;
}


os_info
Machine::OSInfo() const
{
	return fKernelInfo;
}



// private
bool
Machine::_GetDMIDecodeData()
{
	try {
		popen_streambuf dmi("dmidecode", "r");
		std::istream iStream(&dmi);

		std::string string;
		while (std::getline(iStream, string) > 0) {
			// Skip the line with "Handle" in it.
			if (string.find("Handle") == std::string::npos) {
				std::string header = string;
				header = trim(header);

				_GetSystemInfo(iStream, header);
			}
		}
	} catch (...) {
		return false;
	}

	return true;
}


bool
Machine::_GetLSHWData()
{
	// TODO: Parse the output of lshw.
	return false;
}


void
Machine::_GetCPUInfo()
{
	// TODO: Use ProcReader when it's ready for this
	popen_streambuf cpu("cat /proc/cpuinfo", "r");
	std::istream iStream(&cpu);

	std::string string;
	int processorNum = 0;
	while (std::getline(iStream, string) > 0) {
		if (string.find("processor") != std::string::npos) {
			fNumCPUs++;

			// Get the processor number
			size_t pos = string.find(":");
			if (pos == std::string::npos)
				continue;

			std::string valueString = string.substr(pos + 2, std::string::npos);
			trim(valueString);
			processorNum = ::strtol(valueString.c_str(), NULL, 10);
		} else {
			size_t pos = string.find(":");
			if (pos == std::string::npos)
				continue;

			try {
				std::string name = string.substr(0, pos);
				std::string value = string.substr(pos + 1, std::string::npos);

				fCPUInfo[processorNum][trim(name)] = trim(value);

			} catch (...) {
			}
		}
	}
}


void
Machine::_GetOSInfo()
{
	struct utsname uName;
	if (::uname(&uName) != 0)
		throw errno;

	fKernelInfo.hostname = uName.nodename;
	fKernelInfo.comments = uName.version;
	fKernelInfo.os_release = uName.release;
	fKernelInfo.domain_name = uName.domainname;

	ProcReader proc("meminfo");
	std::istream stream(&proc);

	std::string string;
	size_t pos;
	while (std::getline(stream, string) > 0) {
		if ((pos = string.find("SwapTotal:")) != std::string::npos) {
			pos = string.find(":");
			std::string swapString = string.substr(pos + 1, std::string::npos);

			int swapInt = ::strtol(trim(swapString).c_str(), NULL, 10) / 1024;
			std::ostringstream stringStream;
			stringStream << swapInt;
			fKernelInfo.swap = stringStream.str();
		} else if ((pos = string.find("MemTotal:")) != std::string::npos) {
			pos = string.find(":");
			std::string memString = string.substr(pos + 1, std::string::npos);
			int memInt = ::strtol(trim(memString).c_str(), NULL, 10) / 1024;
			std::ostringstream stringStream;
			stringStream << memInt;
			fKernelInfo.memory = stringStream.str();
		}
	}

	_IdentifyOS();
}


void
Machine::_IdentifyOS()
{
	popen_streambuf lsb;
	lsb.open("lsb_release -a", "r");

	std::istream lsbStream(&lsb);
	std::string line;
	while (std::getline(lsbStream, line) > 0) {
		size_t pos = line.find(':');
		if (pos != std::string::npos) {
			std::string key = line.substr(0, pos);
			if (key == "Description") {
				std::string value = line.substr(pos + 1, std::string::npos);
				fKernelInfo.os_description = trim(value);
			}
		}
	}

	if (fKernelInfo.os_description == "") {
		// there is no lsb_release command.
		// try to identify the system in another way
		if (::access("/etc/thinstation.global", F_OK) != -1)
			fKernelInfo.os_description = "Thinstation";
		else
			fKernelInfo.os_description = "Unknown";
	}

}


void
Machine::_GetSystemInfo(std::istream& stream, std::string header)
{
	//std::cout << header << std::endl;
	std::string string;
	size_t pos = 0;
	while (std::getline(stream, string) > 0) {
		//std::cout << "*** " << string << "*** " << std::endl;
		if (string == "")
			break;

		pos = string.find(":");
		if (pos == std::string::npos)
			continue;

		try {
			std::string name = string.substr(0, pos);
			trim(name);
			// TODO: We should prepend the header, actually
			name.append(header);
			std::string value = string.substr(pos + 2, std::string::npos);

			fSystemInfo[trim(name)] = trim(value);

		} catch (...) {

		}
	}
}


std::string
Machine::_GetValue(std::string string, std::string header) const
{
	std::map<std::string, std::string>::const_iterator i;
	i = fSystemInfo.find(string.append(header));
	if (i != fSystemInfo.end())
		return i->second;

	return "";
}



std::string
Machine::_ProcessorInfo(const char* info, int num) const
{
	if (num < 0 || num >= fNumCPUs)
		return "";

	std::map<std::string, std::string>::const_iterator i;
	i = fCPUInfo[num].find(info);
	if (i != fCPUInfo[num].end())
		return i->second;
	return "";
}
