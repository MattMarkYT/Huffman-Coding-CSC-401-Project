// Make a cmake project/target with a set of settings to allow inheriting include/source directories for child projects
// has internal src and include folders
// Optionally add inheritance from other targets made using this program (assumes that it's made with this program)


#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <filesystem>
#include <limits>
#include <vector>
using namespace std;


string inputLine(const string& txt) {
	cout << "\n" << "(Optional) "<< txt;
	string temp;
	getline(cin, temp);
	return temp;
}

string input(const string& txt) {

	cout << "\n"<<txt;
	string temp;
	getline(cin, temp);
	if (temp.empty()) {
		temp = input(txt);
	}

	return temp;
}

bool yesNo(const string& txt) {
	cout << "\n" << txt<<" (Y/n) (default: Y): ";

	string choice;
	getline(cin, choice);

	std::transform(choice.begin(), choice.end(), choice.begin(), ::toupper);

	if (choice.empty() || choice == "Y") {
		return true;
	}
	if (choice == "N") {
		return false;
	}

	yesNo(txt);

}


int main()
{
	
	string dirname = input("Enter the name of the directory for the new project/target: ");
	string targetname = inputLine("Enter the internal name of the target (if different): ");
	string pathcode = input("Target will have internal structure of src/header files;\nIt will use the 'src/<path-code>/' for path of source files, and similar for includes.\nEnter the path code for the target: ");

	
	if (targetname.empty()) {
		targetname = dirname;
	}

	string libTargetName = targetname + "_incLib";
	
	cout << endl<<"Folder structure: " << endl
		<< dirname << "/" << endl
		<< dirname << "/CMakeLists.txt" << endl
		<< dirname << "/main.cpp" << endl
		<< dirname << "/src/" << pathcode << "/" << endl
		<< dirname << "/include/" << pathcode << "/" << endl;


	vector<string> deps;

	cout << endl<<"Enter the target names of projects as dependencies (produced by this program) for this project(enter '_end' to stop): "<<endl;
	string temp;
	while (true) {
		cout << "* Enter the name of the dependency (target name): ";
		cin >> temp;

		if (temp == "_end") {
			break;
		}
		cout << "Added " << temp<<" ";
		if (temp.find("_incLib") == std::string::npos) {
			temp.append("_incLib");
			cout << "(" << temp << ") ";
		}
		cout << "as a dependency" << endl;

		deps.push_back(temp);


	}

	std::filesystem::create_directories("./" + dirname + "/src/" + pathcode);
	std::filesystem::create_directories("./" + dirname + "/include/" + pathcode);



	fstream fout("./" + dirname + "/CMakeLists.txt", std::ios_base::out);
	fout << "add_library(" << libTargetName << " INTERFACE)" << endl << endl
		<< "target_include_directories(" << libTargetName << " INTERFACE" << endl
		<< "  ${CMAKE_CURRENT_SOURCE_DIR}/include/" << endl
		<< ")" << endl;

	fout << endl << endl<< "file(GLOB_RECURSE " << libTargetName << "_SOURCES CONFIGURE_DEPENDS" << endl
		<< "\"${CMAKE_CURRENT_LIST_DIR}/src/huffman/*.cpp\"" << endl
		<< ")" << endl << endl
		<< "target_sources(" << libTargetName << " INTERFACE ${" << libTargetName << "_SOURCES})" << endl;


	fout << endl << endl << "add_executable(" << targetname << " main.cpp)" << endl
		<<"target_link_libraries(" << targetname << " PUBLIC " << libTargetName << ")" << endl;

	for (int i = 0; i < deps.size(); i++) {
		fout << endl<< "target_link_libraries(" << targetname << " PUBLIC " << deps[i] << ")";
	}
	

	fout.close();

	fout.open("./" + dirname + "/main.cpp", std::ios_base::out);

	fout << "#include <iostream>" << endl
		<< endl
		<< "int main(){" << endl
		<< "	std::cout<<\"Hello " << dirname << "!\"<<std::endl;" << endl
		<< "}" << endl;

	fout.close();


	cout << "Files/structure generated!" << endl
		<< "Add 'add_subdirectory (\"" << dirname << "\")' to the main CMakeLists.txt to have it recognised." << endl;


		


	cout << "Press any key to continue. . ." << endl;
	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	std::cin.get();
		


	return 0;
}
