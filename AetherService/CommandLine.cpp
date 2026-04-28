#include "stdafx.h"
#include "CommandLine.h"

#define LOG_MODULE "CommandLine"
#include "Logger.h"




CommandLine::CommandLine(string text) {
	this->line = text;
	this->Parse(text);
}




CommandLine::~CommandLine() {
}




bool CommandLine::is(string command) {

	string match = this->command;
	transform(command.begin(), command.end(), command.begin(), ::tolower);
	transform(match.begin(), match.end(), match.begin(), ::tolower);

	if (command.compare(match) == 0) {
		return true;
	}
	return false;
}





int CommandLine::Parse(string line) {

	string item = "";
	vector<string> items;

	int lineLength = line.size();
	int itemLength = 0;
	int itemCount = 0;
	int index = 0;

	char currentChar;
	char previousChar = 0;
	char splitChars[] = " ,:(){}[]";
	char endChars[] = { '\r', '\n', ';', 0 };
	char commentChar = '#';
	char escapeChar = '\\';
	char enclosingChar = '"';

	bool isSplitChar = false;
	bool isEndChar = false;
	bool isEnclosingChar = false;
	bool isLastChar = false;
	bool isEnclosed = false;

	for (std::string::iterator it = line.begin(); it != line.end(); ++it) {
		currentChar = *it;

		
		if (!isEnclosed && currentChar == commentChar) {
			if (itemLength > 0) {
				items.push_back(item);
			}
			break;
		}

		
		isSplitChar = false;
		for (int i = 0; i < (int)sizeof(splitChars); i++) {
			if (splitChars[i] && currentChar == splitChars[i]) {
				isSplitChar = true;
				break;
			}
		}

		
		isEndChar = false;
		for (int i = 0; i < (int)sizeof(endChars); i++) {
			if (currentChar == endChars[i]) {
				isEndChar = true;
				break;
			}
		}

		
		isLastChar = false;
		if (index == lineLength - 1) {
			isLastChar = true;
		}


		
		isEnclosingChar = false;
		if (currentChar == enclosingChar && previousChar != escapeChar) {
			isEnclosed = !isEnclosed;
			isEnclosingChar = true;
		}

		
		if (
			!isEnclosed &&
			(
				isSplitChar ||
				isEndChar ||
				(itemCount == 0 && currentChar == '=') ||
				(itemCount == 1 && itemLength == 0 && currentChar == '=') ||
				isLastChar ||
				(isLastChar && isEnclosingChar)
				)
			) {

			
			
			

			
			if (isLastChar && !isEndChar && !isSplitChar) {
				if (!isEnclosingChar) {
					item.push_back(currentChar);
					itemLength = 1;
				}
			}

			
			if (itemLength > 0) {
				items.push_back(item);
				item = "";
				itemLength = 0;
				itemCount++;
			}
			else {
				item = "";
				itemLength = 0;
			}

			
			if (isEndChar) {
				break;
			}

			
		}
		else if (currentChar >= 32) {
			if (itemCount == 0 && currentChar == '=') {
			}
			else if (isEnclosingChar) {
			}
			else if (currentChar == escapeChar && !isEnclosed) {
			}
			else {
				item.push_back(currentChar);
				itemLength++;
			}
		}
		index++;
		previousChar = currentChar;
	}

	
	if (itemCount > 0) {
		command = items[0];
		isValid = true;
	}
	else {
		isValid = false;
	}

	
	values.clear();
	for (int i = 1; i < (int)items.size(); i++) {
		values.push_back(items[i]);
	}

	valueCount = values.size();
	return valueCount;
}





string CommandLine::ParseHex(string str) {
	if (str.size() >= 3 && str[0] == '0' && str[1] == 'x') {
		try {
			string tmp = str.substr(2, str.size() - 2);
			return to_string(stol(tmp, 0, 16));
		}
		catch (exception) {
		}
	}
	return str;
}





string CommandLine::GetString(int index, string defaultValue) {
	if (index < valueCount) {
		return values[index];
	}
	return defaultValue;
}





string CommandLine::GetStringLower(int index, string defaultValue) {
	string str = GetString(index, defaultValue);
	transform(str.begin(), str.end(), str.begin(), ::tolower);
	return str;
}





int CommandLine::GetInt(int index, int defaultValue) {
	if (index < valueCount) {
		try {
			auto value = stoi(ParseHex(values[index]));
			return value;
		}
		catch (exception) {}
	}
	return defaultValue;
}





long CommandLine::GetLong(int index, long defaultValue) {
	if (index < valueCount) {
		try {
			auto value = stol(ParseHex(values[index]));
			return value;
		}
		catch (exception) {}
	}
	return defaultValue;
}





double CommandLine::GetDouble(int index, double defaultValue) {
	if (index < valueCount) {
		try {
			auto value = stod(ParseHex(values[index]));
			return value;
		}
		catch (exception) {}
	}
	return defaultValue;
}





float CommandLine::GetFloat(int index, float defaultValue) {
	if (index < valueCount) {
		try {
			auto value = stof(ParseHex(values[index]));
			return value;
		}
		catch (exception) {}
	}
	return defaultValue;
}





bool CommandLine::GetBoolean(int index, bool defaultValue) {
	if (GetInt(index, 0) > 0) return true;
	string str = GetStringLower(index, "");
	if (str == "true") return true;
	if (str == "on") return true;
	if (str == "false") return false;
	if (str == "off") return false;
	if (str == "0") return false;
	return defaultValue;
}
