#include <string>
#include <iostream>
#include <algorithm>
#include <map>
#include <fstream>
#include <cstring>
#include <cstdlib>
using std::cout;
using std::cerr;
using std::endl;
using std::fstream;


class FileManager {

public:

	FileManager() {


	}



	bool validateFile(std::string_view fname) {

		filename = fname;



		file.open(filename.data(), fstream::binary | fstream::in);
		if (!file.good()) {
			displayError("could not open the file");
			return false;
		}

		file.seekg(0, fstream::end);

		int length = file.tellg();
		file.seekg(0, fstream::beg);


		// for now
		if (length < minSIze) {
			displayError("wrong format, below minimum size.");
			return false;
		}

		// check magic
		unsigned char tmp = NULL;
		for (int i = 0; i < sizeof(magic) ; i++) {
			tmp = file.get();

			if (tmp != magic[i]) {
				cerr << "Error occurred when parsing the file " << filename << "; wrong format, magic doesn't match." << endl;
				return false;
			}
		}

		// read type
		//scrap my idea; I forgot that bool mem layout is implementation-defined
		type = file.get();

		
	}

private:

	void displayError(const std::string& msg) {
		cerr<<"Error occurred when reading the file " << filename <<": " << msg <<"."<< endl;
	}
	

	fstream file;
	std::string filename;
	bool type;
	static constexpr unsigned char magic[] = { 0xCCu, 0x40u, 0x1Cu, 0xDFu };

	//increase as the file format parsing develops;
	static constexpr int minSIze = 5;


};


constexpr std::string_view boolToStr(const bool val) noexcept {
	return (val) ? "true" : "false";
}





int main(){
	std::cout<<"Hello FileManager!"<<endl;

	FileManager myfm;

	cout << "Enter the file name: ";

	std::string name;

	getline(std::cin, name);

	cout << "Validating file " << name << endl;

	bool res = myfm.validateFile(name);

	cout << "File validated: " << boolToStr(res) << endl;
	


}
