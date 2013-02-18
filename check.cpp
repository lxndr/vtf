#include <assert.h>
#include <fstream>
#include <iostream>
#include "vtf.h"


int main (int argc, char* argv[])
{
	assert(argc >= 2);
	
	Vtf::File* vtf = new Vtf::File;
	
	try {
		vtf->load(argv[1]);
		Vtf::HiresImageResource* img = dynamic_cast<Vtf::HiresImageResource*>(
				vtf->findResource(Vtf::Resource::TypeHires));
		std::cout << "Format: " << img->getFormat() << std::endl
				<< "Width: " << img->getWidth() << std::endl
				<< "Height: " << img->getHeight() << std::endl
				<< "Depth: " << img->getDepth() << std::endl
				<< "Frames: " << img->getFrameCount() << std::endl;
		uint8_t* data = img->getImageRGBA(0, 0, 0, 0);
		if (data)
			delete[] data;
	} catch (std::ifstream::failure e) {
		std::cout << "Exception opening/reading file" << std::endl;
	} catch (std::exception& e) {
		std::cout << e.what() << std::endl;
	}
	
	delete vtf;
	return 0;
}
