﻿#include "pch.h"
#include "ProcessManager.h"
#include "FileSystem.h"

int ProcessManager::last_process_id_ = 0;

ProcessManager::ProcessManager()
{
	InitializeProcessPrinter();
	CreateDummyProcess();
}

ProcessManager::~ProcessManager()
{
	for (auto&& process : processes_)
		delete process;
	processes_.clear();
}

void ProcessManager::RemoveFromVector(Process* process, std::vector<Process*>& vector) const
{
	vector.erase(std::remove(vector.begin(), vector.end(), process), vector.end());
}

void ProcessManager::SetProcessNew(Process* process)
{
	process->set_process_state(Process::New);
	new_processes_.push_back(process);

	std::sort(new_processes_.begin(), new_processes_.end(), [](Process* process1, Process* process2) { return process1->priority() > process2->priority(); });
}

void ProcessManager::CreateDummyProcess()
{
	FileSystem::GetInstance().create("dummy", ".text NP .data 0");
	const auto process_code = FileSystem::GetInstance().read_all("dummy");
	dummy_process_ = new Process("dummy", "dummy", 0, ++last_process_id_);
	processes_.push_back(dummy_process_);

	SetProcessNew(dummy_process_);
	VirtualMemory::GetInstance().create_program(dummy_process_, process_code);

	try
	{
		RAM::GetInstance().add_to_RAM(dummy_process_);
		SetProcessReady(dummy_process_);
	}
	catch (std::exception & e)
	{
		std::cout << e.what();
	}

	SetProcessRunning(dummy_process_);
}

Process* ProcessManager::CreateProcess(std::string process_name, std::string process_file, const int priority)
{
	if (Exists(process_name))
		throw std::exception("Process with given name already exists");

	const auto process_code = FileSystem::GetInstance().read_all(process_file);
	const auto process = new Process(process_name, process_file, priority, ++last_process_id_);

	try
	{
		VirtualMemory::GetInstance().create_program(process, process_code);
	}
	catch(std::exception& e)
	{
		delete process;
		throw e;
	}

	processes_.push_back(process);
	SetProcessNew(process);
	
	try
	{
		RAM::GetInstance().add_to_RAM(process);
		SetProcessReady(process);
	}
	catch (std::exception & e)
	{
		std::cout << e.what();
	}

	CPU_M::GetInstance().scheduling();

	return process;
}

void ProcessManager::CloseProcessFiles(Process* process)
{
	for (auto& file_name : process->opened_files())
		FileSystem::GetInstance().close(file_name);
}

void ProcessManager::RemoveProcessFromMemory(Process* process)
{
	try
	{
		RAM::GetInstance().delete_from_RAM(process);
	}
	catch (std::exception & e)
	{
	}
	VirtualMemory::GetInstance().delete_program(process);
}

void ProcessManager::TryAllocateNewProcesses()
{
	for (auto& process : new_processes_)
	{
		try
		{
			RAM::GetInstance().add_to_RAM(process);
			SetProcessReady(process);
		}
		catch (std::exception & e)
		{
		}
	}
}

void ProcessManager::ChangeProcessPriority(std::string process_name, int priority)
{
	ChangeProcessPriority(GetProcess(process_name), priority);
}

void ProcessManager::ChangeProcessPriority(Process* process, int priority)
{
	process->set_priority(priority);
	CPU_M::GetInstance().scheduling();
}

void ProcessManager::KillProcess(Process* process)
{
	if (process == dummy_process_)
		throw std::exception("Proces dummy nie moze byc zabity");
	
	switch (process->process_state())
	{
	case Process::New:
		RemoveFromVector(process, new_processes_); break;
	case Process::Waiting:
		throw std::exception("Proces oczekujacy nie moze byc zabity"); break;
	case Process::Ready:
		RemoveFromVector(process, ready_processes_); break;
	case Process::Running:
		running_process_ = nullptr;
		SetProcessRunning(dummy_process_);
		break;
	default: break;
	}
	
	RemoveFromVector(process, processes_);
	RemoveProcessFromMemory(process);
	CloseProcessFiles(process);

	TryAllocateNewProcesses();

	CPU_M::GetInstance().scheduling();

	delete process;
}

void ProcessManager::KillProcess(int process_id)
{
	KillProcess(GetProcess(process_id));
}

void ProcessManager::KillProcess(std::string process_name)
{
	KillProcess(GetProcess(process_name));
}

bool ProcessManager::Exists(std::string process_name)
{
	return std::find_if(processes_.begin(), processes_.end(), [process_name](Process* process) { return process->name() == process_name; }) != processes_.end();
}

Process* ProcessManager::GetProcess(int process_id)
{
	auto process = std::find_if(processes_.begin(), processes_.end(), [process_id](Process* process) { return process->id() == process_id; });

	if (process == processes_.end())
		throw std::exception("Proces nie istnieje");
	
	return *process;
}

Process* ProcessManager::GetProcess(std::string process_name)
{
	auto process = std::find_if(processes_.begin(), processes_.end(), [process_name](Process* process) { return process->name() == process_name; });

	if (process == processes_.end())
		throw std::exception("Proces nie istnieje");

	return *process;
}

void ProcessManager::SetProcessWaiting(Process* process)
{
	switch (process->process_state())
	{
	case Process::New:
		RemoveFromVector(process, new_processes_); break;
	case Process::Waiting:
		return;
	case Process::Ready:
		RemoveFromVector(process, ready_processes_); break;
	case Process::Running:
		break;
	default: break;
	}

	process->set_process_state(Process::Waiting);
	waiting_processes_.push_back(process);

	dummy_process_->set_process_state(Process::Running);
	running_process_ = dummy_process_;
	CPU_M::GetInstance().scheduling();
}

void ProcessManager::SetProcessWaiting(int process_id)
{
	SetProcessWaiting(GetProcess(process_id));
}

void ProcessManager::SetProcessWaiting(std::string process_name)
{
	SetProcessWaiting(GetProcess(process_name));
}

void ProcessManager::SetProcessReady(Process* process)
{
	switch (process->process_state())
	{
	case Process::New:
		RemoveFromVector(process, new_processes_); break;
	case Process::Waiting:
		RemoveFromVector(process, waiting_processes_); break;
	case Process::Ready:
		return;
	case Process::Running:
		//SetProcessRunning(dummy_process_);
		break;
	default: break;
	}

	process->set_process_state(Process::Ready);
	ready_processes_.push_back(process);
}

void ProcessManager::SetProcessReady(int process_id)
{
	SetProcessReady(GetProcess(process_id));
}

void ProcessManager::SetProcessReady(std::string process_name)
{
	SetProcessReady(GetProcess(process_name));
}

void ProcessManager::SetProcessRunning(Process* process)
{
	switch(process->process_state())
	{
	case Process::New:
		RemoveFromVector(process, new_processes_); break;
	case Process::Waiting:
		RemoveFromVector(process, waiting_processes_); break;
	case Process::Ready:
		RemoveFromVector(process, ready_processes_); break;
	case Process::Running:
		return;
	default: break;
	}

	if(running_process_ != nullptr)
		SetProcessReady(running_process_);

	process->set_process_state(Process::Running);
	running_process_ = process;
}

void ProcessManager::SetProcessRunning(int process_id)
{
	SetProcessRunning(GetProcess(process_id));
}

void ProcessManager::SetProcessRunning(std::string process_name)
{
	SetProcessRunning(GetProcess(process_name));
}

void ProcessManager::InitializeProcessPrinter()
{
	process_printer_.AddColumn("ID", 2);
	process_printer_.AddColumn("Nazwa", 10);
	process_printer_.AddColumn("Plik", 10);
	process_printer_.AddColumn("Priorytet", 2);
	process_printer_.AddColumn("Stan", 7);
	process_printer_.AddColumn("AX", 3);
	process_printer_.AddColumn("BX", 3);
	process_printer_.AddColumn("CX", 3);
	process_printer_.AddColumn("DX", 3);
	process_printer_.AddColumn("IC", 3);
}

void ProcessManager::PrintProcesses(std::vector<Process*> processes)
{
	process_printer_.PrintHeader();
	for (auto process : processes)
		process_printer_ << process->id() << process->name() << process->file_name() << process->priority() << process->process_state() << process->ax() << process->bx() << process->cx() << process->dx() << process->instruction_counter();
	process_printer_.PrintFooter();
}

void ProcessManager::PrintProcesses()
{
	PrintProcesses(processes_);
}

void ProcessManager::PrintProcessOpenedFiles(Process* process)
{
	process->print_opened_files();
}

void ProcessManager::PrintProcessOpenedFiles(std::string process_name)
{
	const auto process = GetProcess(process_name);
	PrintProcessOpenedFiles(process);
}

void ProcessManager::PrintProcess(Process* process) 
{
	process_printer_.PrintHeader();
	process_printer_ << process->id() << process->name() << process->file_name() << process->priority() << process->process_state() << process->ax() << process->bx() << process->cx() << process->dx() << process->instruction_counter();
	process_printer_.PrintFooter();
}

void ProcessManager::PrintProcess(int process_id) 
{
	PrintProcess(GetProcess(process_id));
}

void ProcessManager::PrintProcess(std::string process_name)
{
	PrintProcess(GetProcess(process_name));
}

void ProcessManager::OpenFile(Process* process, std::string file_name)
{
	if (process->is_file_opened(file_name))
		throw std::exception("Plik jest juz otwarty");

	FileSystem::GetInstance().open(process, file_name);
}

void ProcessManager::CloseFile(Process* process, std::string file_name)
{
	if (!process->is_file_opened(file_name))
		throw std::exception("Process cannot close not opened file.");

	FileSystem::GetInstance().close(file_name);
	process->remove_opened_file(file_name);
}

void ProcessManager::WriteFile(Process* process, std::string file_name, std::string bytes, bool append)
{
	if(!process->is_file_opened(file_name))
		throw std::exception("Proces nie moze zapisac danych do nieotwartego pliku");

	FileSystem::GetInstance().write(file_name, bytes, append);
}

std::string ProcessManager::ReadFile(Process* process, std::string file_name)
{
	if(!process->is_file_opened(file_name))
		throw std::exception("Proces nie moze odczytac nieotwartego pliku");

	return FileSystem::GetInstance().read_all(file_name);
}

char ProcessManager::ReadFileByte(Process* process, std::string file_name)
{
	if(!process->is_file_opened(file_name))
		throw std::exception("Proces nie moze odczytac nieotwartego pliku");

	return FileSystem::GetInstance().read_next_byte(file_name);
}

void ProcessManager::ResetFilePointer(Process* process, std::string file_name)
{
	if(!process->is_file_opened(file_name))
		throw std::exception("Proces nie moze zresetowac nieotwartego pliku");

	FileSystem::GetInstance().reset_last_read_byte(file_name);
}

std::vector<Process*> ProcessManager::processes() const
{
	return processes_;
}

std::vector<Process*> ProcessManager::new_processes() const
{
	return new_processes_;
}

std::vector<Process*> ProcessManager::waiting_processes() const
{
	return waiting_processes_;
}

std::vector<Process*> ProcessManager::ready_processes() const
{
	return ready_processes_;
}

Process* ProcessManager::running_process() const
{
	return running_process_;
}
