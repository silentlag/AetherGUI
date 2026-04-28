#include "stdafx.h"
#include "Logger.h"




Logger::Logger() {
	verbosity = LogLevelDebug;
	newMessage = false;
	directPrint = false;
}


void Logger::OutputMessage(LogItem *message) {
	char timeBuffer[64];
	char moduleBuffer[128];

	strftime(timeBuffer, 64, "%Y-%m-%d %H:%M:%S", &message->time);
	if(message->module.length() > 0) {
		sprintf_s(moduleBuffer, " [%s]", message->module.c_str());
	} else {
		moduleBuffer[0] = 0;
	}

	try {
		cout << timeBuffer << moduleBuffer << " [" << levelNames[message->level] << "] " << message->text << flush;
	} catch(exception) {
		exit(1);
	}
	if(logFile && logFile.is_open()) {
		logFile << timeBuffer << moduleBuffer << " [" << levelNames[message->level] << "] " << message->text << flush;
	}
}




void Logger::ProcessMessages() {

	
	lockMessages.lock();

	
	vector<LogItem> tmp(messages);
	messages.clear();

	
	lockMessages.unlock();

	
	for(auto message : tmp) {
		if(!directPrint) {
			OutputMessage(&message);
		}
	}
}





void Logger::LogMessage(int level, string module, const char *fmt, ...) {
	char message[4096];
	int maxLength = sizeof(message) - 1;
	int index;
	message[0] = 0;

	
	if(level < 2)
		level = 2;
	else if(level > 8)
		level = 8;

	if(level <= verbosity) {
		index = 0;

		
		va_list ap;
		va_start(ap, fmt);
		if(index < maxLength) index += vsnprintf(message + index, maxLength - index, fmt, ap);
		va_end(ap);

		
		if(index >= maxLength) {
			message[maxLength - 1] = '\n';
			message[maxLength] = 0;
		}

		
		time_t t;
		time(&t);

		
		LogItem logItem;
		localtime_s(&logItem.time, &t);
		logItem.level = level;
		logItem.module = module;
		logItem.text = message;
		AddMessage(&logItem);
	}
}




void Logger::LogBuffer(int level, string module, void *buffer, int length, const char *fmt, ...) {
	bool newLine = false;
	char message[4096];
	int maxLength = sizeof(message) - 1;
	time_t t;
	int index;
	message[0] = 0;

	
	if(level < 2)
		level = 2;
	else if(level > 7)
		level = 7;

	if(level <= verbosity) {
		index = 0;
		time(&t);

		
		va_list ap;
		va_start(ap, fmt);
		if(index < maxLength) index += vsnprintf(message + index, maxLength - index, fmt, ap);
		va_end(ap);

		
		newLine = (fmt[strlen(fmt) - 1] == '\n');
		if(newLine) {
			if(index < maxLength) index += snprintf(message + index, maxLength - index, "  { ");

		} else {
			if(index < maxLength) index += snprintf(message + index, maxLength - index, "{ ");
		}


		
		for(int i = 0; i < length; i++) {

			
			if(i == length - 1) {
				if(index < maxLength) index += snprintf(message + index, maxLength - index, "0x%02x", ((unsigned char*)buffer)[i]);

				
			} else {
				if(index < maxLength) index += snprintf(message + index, maxLength - index, "0x%02x, ", ((unsigned char*)buffer)[i]);

			}
			
			if(newLine && (i + 1) % 12 == 0 && i != length - 1) {
				if(index < maxLength) index += snprintf(message + index, maxLength - index, "\n    ");
			}
		}

		
		if(index < maxLength) index += snprintf(message + index, maxLength - index, " }\n");

		
		if(index >= maxLength) {
			message[maxLength - 1] = '\n';
			message[maxLength] = 0;
		}

		
		LogItem logItem;
		localtime_s(&logItem.time, &t);
		logItem.level = level;
		logItem.module = module;
		logItem.text = message;
		AddMessage(&logItem);
	}
}




void Logger::AddMessage(LogItem *message) {

	
	if(directPrint) {
		OutputMessage(message);
	}

	
	lockMessages.lock();

	
	messages.push_back(*message);

	
	lockMessages.unlock();

	newMessage = true;
}





void Logger::run() {
	while(true) {

		
		if(newMessage) {

		
			newMessage = false;

			
			ProcessMessages();
		}

		
		if(!isRunning && !newMessage) break;

		
		Sleep(2);
	}
}




bool Logger::OpenLogFile(string filename) {
	if(logFile && logFile.is_open()) {
		logFile.close();
	}
	logFile = ofstream(filename, ofstream::out);
	if(!logFile) {
		return false;
	}
	logFilename = filename;
	return true;
}




bool Logger::CloseLogFile() {
	if(logFile && logFile.is_open()) {
		logFile.close();
		return true;
	}
	return false;
}





void Logger::Start() {
	if(!isRunning) {
		isRunning = true;
		threadLog = thread([this] { this->run(); });
	}
}




void Logger::Stop() {
	if(isRunning) {
		isRunning = false;
		newMessage = true;
		threadLog.join();
		if(logFile && logFile.is_open()) {
			logFile.close();
		}
	}
}

Logger logger;
