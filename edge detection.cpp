#include <iostream> 
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <math.h>

using namespace std;

#define BYTE unsigned char //1 byte
#define WORD unsigned short //2 byte
#define DWORD unsigned int //4 byte

#pragma pack(push)
#pragma pack(2)
struct BitmapFileHeader //data sturcture to record the data of Bitmap File Header
{
	WORD bfType;
	DWORD bfSize;
	WORD bfReserved1;
	WORD bfReserved2;
	DWORD bfOffBits;
};
#pragma pack(pop)

struct BitmapInfoHeader //data sturcture to record the data of Bitmap File Header
{
	DWORD biSize;
	int biWidth;
	int biHeight;
	WORD biPlanes;
	WORD biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	int biXPelsPerMeter;
	int biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
};

class Picture
{
	public:
		int width, height;
		int rowSize;
		BYTE* term; //address of RGB value matrix
		 
	Picture()
	{
		width = height = 0;
	}
	
	Picture(int w, int h)
	{
		this->width = w;
		this->height = h;
		rowSize = ( 3 * this->width + 3 ) / 4 * 4; //row size must be a multiple of 4
		term = new BYTE[height*rowSize];
		for(int i=0 ; i<height*rowSize ; ++i)
			term[i] = 0;
	}
	
	//load the information from the file, and assign them to this object
	bool loadFile(char* filename)
	{
		BitmapFileHeader bfheader;
		BitmapInfoHeader bInfoheader;
		ifstream f;
		f.open(filename, ios::binary);
		f.read((char*)&bfheader, sizeof(bfheader)); //read 1 byte each time
		f.read((char*)&bInfoheader, sizeof(bInfoheader));
		
		//check the format of opened file
		if(bfheader.bfType != 0x4d42 
			|| bInfoheader.biClrUsed!=0
			|| bInfoheader.biBitCount!=24
			|| bInfoheader.biCompression!=0
			|| bInfoheader.biPlanes!=1)
		{
			cout<<"This format is not supported!\n";
			f.close();
			return 0;
		}
		
		this->width = bInfoheader.biWidth;
		this->height = bInfoheader.biHeight;
		rowSize = ( 3 * this->width + 3 ) / 4 * 4; //row size must be a multiple of 4
		term = new BYTE[height*rowSize];
		
		f.read((char*)term, height*rowSize);
		
		f.close();
		return 1;
	}
	
	//save the data of this object to file
	bool saveFile(char* filename)
	{
		BitmapFileHeader bfheader=
		{
			0x4d42,
			54 + height*rowSize,
			0,
			0,
			54
		};
		
		BitmapInfoHeader bInfoheader=
		{
			40,
			width,
			height,
			1,
			24,
			0,
			height*rowSize,
			3780,
			3780,
			0,
			0
		};
		
		ofstream f;
		f.open(filename, ios::binary);
		f.write((char*)&bfheader, 14);
		f.write((char*)&bInfoheader, 40);
		f.write((char*)term, height*rowSize);
		f.close();
		return 1;
	}
};

//transfer the RGB image to grayscale
Picture toGray(Picture p)
{
	Picture newP = Picture(p.width, p.height);
	for(int y=0 ; y < p.height ; ++y)
	{
		for(int x=0 ; x < p.rowSize ; x+=3)
		{
			BYTE b = p.term[y*p.rowSize + x];
			BYTE g = p.term[y*p.rowSize + x + 1];
			BYTE r = p.term[y*p.rowSize + x + 2];
			newP.term[y*p.rowSize + x] = 0.299*r + 0.587*g + 0.114*b;
			newP.term[y*p.rowSize + x + 1] = 0.299*r + 0.587*g + 0.114*b;
			newP.term[y*p.rowSize + x + 2] = 0.299*r + 0.587*g + 0.114*b;
		}
	}
	return newP;
}

Picture histoEqual(Picture p) //p must be gray level image
{	
	Picture newP = Picture(p.width, p.height);
	
	float pr[256];  //pr[k] = nk/MN
	int s[256]; //Histogram equalization: change the gray level from k to s[k]
	
	
	int count[256], totalNum = 0;
	for(int i=0 ; i<256 ; ++i)
		count[i] = 0;
	
	for(int y=0 ; y < p.height ; ++y)
	{
		for(int x=0 ; x < p.rowSize ; x+=3)
		{
			count[(int)p.term[y*p.rowSize + x]] += 1; //count the number of each gray level value
			totalNum += 1;	 //total pixels number
		}
	}
	
	//compute pr
	for(int i=0 ; i<256 ; ++i)
	{
		pr[i] = ((float)count[i] / (float)(totalNum));
	}
	
	//compute sk
	float sum;
	for(int i=0 ; i<256 ; ++i)
	{
		sum = 0;
		for(int j=0 ; j<=i ; ++j)
			sum += pr[j];
		
		s[i] = 255*sum;
	}
	
	//Change the pixel distribution of the new picture
	for(int y=0 ; y < p.height ; ++y)
	{
		for(int x=0 ; x < p.rowSize ; x+=3)
		{
			newP.term[y*p.rowSize + x] = (BYTE)s[(int)p.term[y*p.rowSize + x]];
			newP.term[y*p.rowSize + x + 1] = (BYTE)s[(int)p.term[y*p.rowSize + x]];
			newP.term[y*p.rowSize + x + 2] = (BYTE)s[(int)p.term[y*p.rowSize + x]];	
		}
	}
		
	return newP;
}

Picture edgeDetection(Picture p) //p must be gray level image
{
	Picture newP = Picture(p.width, p.height);
	
	//Sobel Operator Mask 
	int Mx[9] = {-1, -2, -1, 0, 0, 0, 1, 2, 1};
	int My[9] = {-1, 0, 1, -2, 0, 2, -1, 0, 1}; 
	
	//Edge Detection
	//Convolution wirh Sobel Operator Mask
	int Gx, Gy;
	int threshold = 100;
	for(int y=0 ; y < p.height-2 ; ++y)
	{
		for(int x=0 ; x < p.rowSize-2*3 ; x+=3)
		{ 
			Gx = Gy = 0;
			for(int m=0 ; m < 3  ; m++)
			{
				for(int n=0 ; n < 3  ; n++)
				{
					Gx += Mx[m*3 +n] * (int)p.term[(y+m)*p.rowSize + (x+3*n)];
					Gy += My[m*3 +n] * (int)p.term[(y+m)*p.rowSize + (x+3*n)];
				}
			}

			//use threshold to make edge more obviously
			if((int)sqrt(Gx*Gx + Gy*Gy) > threshold)
			{
				newP.term[(y+1)*p.rowSize + (x+1)] = 255;
				newP.term[(y+1)*p.rowSize + (x+1)+1] = 255;
				newP.term[(y+1)*p.rowSize + (x+1)+2] = 255;
			}
			else
			{
				newP.term[(y+1)*p.rowSize + (x+1)] = 0;
				newP.term[(y+1)*p.rowSize + (x+1)+1] = 0;
				newP.term[(y+1)*p.rowSize + (x+1)+2] = 0;
			}
		}
	}
	
	return newP;
}

int main(int argc, char* argv[])
{
	if(argc!=4)
		return 0;
		
	char* inputName = argv[1];
	char* outputName = argv[2];
	char* outputName2 = argv[3];
	
	Picture p, pp;
	p.loadFile(inputName);
	p = toGray(p);
	pp = histoEqual(p);
	p = edgeDetection(p);
	p.saveFile(outputName); 
	pp.saveFile(outputName2);
	
	return 0;
}
