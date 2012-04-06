// ASE2OBJ.cpp : Defines the entry point for the console application.
//

//-ase2obj "c:\users\jonathan\desktop\cw-stairs.ase" "D:\metalbox.obj"

//-ase2obj "D:\metalbox.obj" "D:\metalbox2.obj"


#define USERINTERFACE
//#define DEBUG_NORMALS

#if defined(USERINTERFACE)
	#include <windows.h>
#endif


#include "picomodel.h"

#include <cstring>
#include <string.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>

#define obj_newl "\x0A"

using namespace std;

void SafeRead (FILE *f, void *buffer, int count)
{
	if ( (int)fread (buffer, 1, count, f) != count)
		return;
		//Error ("File read failure");
}

#define MEM_BLOCKSIZE 4096
void* qblockmalloc(size_t nSize)
{
	void *b;
  // round up to threshold
  int nAllocSize = nSize % MEM_BLOCKSIZE;
  if ( nAllocSize > 0)
  {
    nSize += MEM_BLOCKSIZE - nAllocSize;
  }
	b = malloc(nSize + 1);
	memset (b, 0, nSize);
	return b;
}

/*
================
Q_filelength
================
*/
int Q_filelength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

/*
==============
LoadFile
==============
*/
int LoadFile (const char *filename, void **bufferptr)
{
	FILE	*f=0;
	int    length;
	void    *buffer;

  *bufferptr = NULL;
  
  if (filename == NULL || strlen(filename) == 0)
  {
    return -1;
  }
	errno_t ferr = fopen_s (&f,filename, "rb");
	if (!f)
	{
		return -1;
	}
	length = Q_filelength (f);
	buffer = qblockmalloc (length+1);
	((char *)buffer)[length] = 0;
	SafeRead (f, buffer, length);
	fclose (f);

	*bufferptr = buffer;
	return length;
}

void picoLoadFile(char* filename, unsigned char** buffer, int* bufSize)
{
	*bufSize = LoadFile(filename,(void**)buffer);
}

#if defined(DEBUG_NORMALS)
struct FaceNormal_s
{
	picoVec3_t v1,v2,v3; // Triangle definition
	picoVec3_t a,b;		 // v2-v1 / v3-v2
	picoVec3_t normal;	// Normal vector

	picoVec3_t nv1,nv2,nv3;	// Normal triangle vectors

	void calcNormal();

} FaceNormal_t;

void FaceNormal_s::calcNormal()
{
	a[0] = v2[0]-v1[0];
	a[1] = v2[1]-v1[1];
	a[2] = v2[2]-v1[2];

	b[0] = v3[0]-v2[0];
	b[1] = v3[1]-v2[1];
	b[2] = v3[2]-v2[2];

	normal[0] = a[1]*b[2] - a[2]*b[1];
	normal[1] = a[2]*b[0] - a[0]*b[2];
	normal[2] = a[0]*b[1] - a[1]*b[0];

	nv1[0] = (v1[0]+v2[0]+v3[0])/3;
	nv1[1] = (v1[1]+v2[1]+v3[1])/3;
	nv1[2] = (v1[2]+v2[2]+v3[2])/3;

	nv2[0] = nv1[0] + 0.1f*(nv1[0]-v1[0]);
	nv2[1] = nv1[1] + 0.1f*(nv1[1]-v1[1]);
	nv2[2] = nv1[2] + 0.1f*(nv1[2]-v1[2]);

	nv3[0] = nv1[0] + normal[0]/100;
	nv3[1] = nv1[1] + normal[1]/100;
	nv3[2] = nv1[2] + normal[2]/100;

}

std::vector<FaceNormal_s> normals;
#endif
void convertAseToObj(char* filein, char* fileout)
{	
	picoModel_t *model;

	printf("Converting\n%s\n%s\n",filein,fileout);

	model = 0;
	model = PicoLoadModel(filein,0);
	if (model != 0)
	{
		// Open file

		char mtlfile[4096];
		char mtlNoFolder[4096];
		strcpy_s(mtlfile,4096,fileout);
		int t = strlen(mtlfile);
		if(t<3) return;
		mtlfile[t-1] = 'l';mtlfile[t-2] = 't';mtlfile[t-3] = 'm';

		{
			char* lastSlash = mtlfile;
			char* p=mtlfile;
			while(*p)
			{
				if((*p == '/') || (*p=='\\' )) lastSlash = p;
				p++;
			}
			lastSlash++;
			strcpy_s(mtlNoFolder,4096,lastSlash);
		}
		
		{
			fstream fmtl(mtlfile,ios_base::out|ios_base::trunc);

			// Write material data

			fmtl << "# picomodel obj converter version 0.1 2012" obj_newl "#" obj_newl;

			for (int nsh=0;nsh<model->numShaders;nsh++)
			{
				picoShader_t* shader = model->shader[nsh];
				// Name
				fmtl << "newmtl " << shader->name << obj_newl;
				// Ambient colour
				fmtl << "Ka " << (float)shader->ambientColor[0] / 255 
					 << " " <<   (float)shader->ambientColor[1] / 255 
					 << " "	<<   (float)shader->ambientColor[2] / 255 << obj_newl;
				// Diffuse colour
				fmtl << "Kd " << (float)shader->diffuseColor[0] / 255 
					 << " " <<   (float)shader->diffuseColor[1] / 255 
					 << " " <<   (float)shader->diffuseColor[2] / 255 << obj_newl;
				// Diffuse map
				if(shader->mapName) 
					fmtl << "map_Kd " << shader->mapName << obj_newl;
				// Finish on a comment
				fmtl << "#" obj_newl;
			}

			fmtl << "# EOF";

			fmtl.close();
		}

		// Then geometry
		{
			int vertexOffset = 1; // The first vertex is 1

			fstream fgeo(fileout,std::ios_base::out|std::ios_base::trunc);

			fgeo << "# picomodel obj converter version 0.1 2012" obj_newl "#" obj_newl;

			fgeo << "mtllib ./" << mtlNoFolder << obj_newl;

			for(int nsu=0;nsu < model->numSurfaces;nsu++)
			{
				picoSurface_t *surface = model->surface[nsu];
				fgeo << "g" << obj_newl;
				fgeo << "# object " << surface->name << " to come..." << obj_newl; // Error here: surface->name is not object name...
				
				fgeo << "#" << obj_newl;

				// Loop through vertex coords				
				for (int ivc=0;ivc<surface->numVertexes;ivc++)
				{
					fgeo << "v " << surface->xyz[ivc][0] << " " << surface->xyz[ivc][1] << " " << surface->xyz[ivc][2] << obj_newl;
				}

				fgeo << "# " << surface->numVertexes << " verticies" << obj_newl << obj_newl;

				// Loop through vertex texture coords
				for (int ivt=0;ivt<surface->numVertexes;ivt++)
				{
					fgeo << "vt " << surface->st[0][ivt][0] << " " << surface->st[0][ivt][1] << obj_newl;
				}
				fgeo << "# " << surface->numVertexes << " texture verticies" << obj_newl << obj_newl;

				// Loop through vertex normals
				for (int ivn=0;ivn<surface->numVertexes;ivn++)
				{
					fgeo << "vn " << surface->normal[ivn][0] 
					<< " " << surface->normal[ivn][1] 
					<< " " << surface->normal[ivn][2] << obj_newl;

					/*fgeo << "vn " << 0 
					<< " " << 0 
					<< " " << 0 << obj_newl;*/
				}
				fgeo << "# " << surface->numVertexes << " vertex normals" << obj_newl << obj_newl;

				// surface name and assign material
				fgeo << "g " << surface->name << obj_newl;
				fgeo << "usemtl " << surface->shader->name << obj_newl;

				unsigned long long int sg = ~0;
				
				int numTris = surface->numIndexes / 3;
				for(int it=0;it<numTris;it++)
				{
					if(surface->smoothingGroup[surface->index[it*3]] != sg) 
					{
						sg = surface->smoothingGroup[surface->index[it*3]];
						fgeo << "sg " << sg << obj_newl;
					}
					fgeo << "f " << surface->index[it*3+2]+vertexOffset << "/" << surface->index[it*3+2]+vertexOffset << "/" << surface->index[it*3+2]+vertexOffset
						 << " "  << surface->index[it*3+1]+vertexOffset << "/" << surface->index[it*3+1]+vertexOffset << "/" << surface->index[it*3+1]+vertexOffset
						 << " "  << surface->index[it*3+0]+vertexOffset << "/" << surface->index[it*3+0]+vertexOffset << "/" << surface->index[it*3+0]+vertexOffset						 
						 << obj_newl;
#if defined(DEBUG_NORMALS)
					{
						FaceNormal_s face;
						face.v1[0] = surface->xyz[surface->index[it*3+2]][0];
						face.v1[1] = surface->xyz[surface->index[it*3+2]][1];
						face.v1[2] = surface->xyz[surface->index[it*3+2]][2];

						face.v2[0] = surface->xyz[surface->index[it*3+1]][0];
						face.v2[1] = surface->xyz[surface->index[it*3+1]][1];
						face.v2[2] = surface->xyz[surface->index[it*3+1]][2];

						face.v3[0] = surface->xyz[surface->index[it*3+0]][0];
						face.v3[1] = surface->xyz[surface->index[it*3+0]][1];
						face.v3[2] = surface->xyz[surface->index[it*3+0]][2];
						face.calcNormal();
						normals.push_back(face);
					}
#endif
				}
				fgeo << "# " << numTris << " faces" << obj_newl << obj_newl;

				/*for( int is=0;is<surface->numVertexes;is++)
				{
					printf("%i\n",surface->smoothingGroup[is]);
				}
				printf("\n");*/

				// Increment the vertex offset
				vertexOffset += surface->numVertexes;
			
			}			

#if defined(DEBUG_NORMALS)
			{
				// Iterate normals
				int normalCnt=1;
				for(std::vector<FaceNormal_s>::iterator iter=normals.begin();iter!=normals.end();iter++)
				{
					fgeo << "g" << obj_newl;
					// write 'v's
					fgeo << "v " << iter->nv1[0] << " " << iter->nv1[1] << " " << iter->nv1[2] << obj_newl;
					fgeo << "v " << iter->nv2[0] << " " << iter->nv2[1] << " " << iter->nv2[2] << obj_newl;
					fgeo << "v " << iter->nv3[0] << " " << iter->nv3[1] << " " << iter->nv3[2] << obj_newl;					
					fgeo << "# 3 verticies" << obj_newl << obj_newl;
					fgeo << "g normal" << normalCnt++ << obj_newl;
					// write 'f' faces
					fgeo << "f " << vertexOffset+0 << " " << vertexOffset+1 << " " << vertexOffset+2 << obj_newl;
					fgeo << "# 1 face" << obj_newl << obj_newl;
					vertexOffset += 3;
				}
			}
#endif

			fgeo << "g";

			fgeo.close();
		}

		//model->surface[i]->xyz
		//model->surface[i]->st
		//model->surface[0]->

		// Triangle count == surface.numIndexes/3
	}
	else
	{
		printf( "ERROR: pico load failed: %s\n", filein );
	}

}

int main(int argc, char* argv[])
{
	printf("ASE(pico) -> OBJ Converter " __DATE__ " " __TIME__ "\n");
	printf("-ase2obj\nv0.1\n");
	// Setup picomodel stuff
	PicoSetLoadFileFunc(picoLoadFile);


#if defined(USERINTERFACE)
	if (argc==1) // no parameters passed
	{
		printf("No params, opening file dialog\n");
		OPENFILENAME ofn;
		char szFileName[MAX_PATH];
		//char path[MAX_PATH];
		//char temp_base[MAX_PATH];
		//std::strcpy(temp_base,systems[0].basePath);
		//strLower(temp_base);

		ZeroMemory(&ofn, sizeof(ofn));
		szFileName[0] = 0;

		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = 0;
		ofn.lpstrFilter = "ASE Model\0*.ase\0All Files (*.*)\0*.*\0\0";
		ofn.lpstrFile = szFileName;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrDefExt = "ase";
		ofn.Flags = OFN_NONETWORKBUTTON | OFN_READONLY | OFN_NOCHANGEDIR /*| OFN_ALLOWMULTISELECT*/;
		if (GetOpenFileName(&ofn) != 0)
		{
			//OPENFILENAME ofn;
			char szSaveFileName[MAX_PATH];
			//char path[MAX_PATH];
			//bool status;

			ZeroMemory(&ofn, sizeof(ofn));
			//szFileName[0] = 0;
			strcpy_s(szSaveFileName,sizeof(szSaveFileName),"");

			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = 0;
			ofn.lpstrFilter = "Obj File (*.obj)\0*.obj\0All Files (*.*)\0*.*\0\0";
			ofn.lpstrFile = szSaveFileName;
			//ofn.lpstrInitialDir = "";
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrDefExt = "obj";
			//ofn.Flags = OFN_NONETWORKBUTTON | OFN_NOCHANGEDIR;
			if (GetSaveFileName(&ofn) != 0)
			{	  
			  convertAseToObj(szFileName,szSaveFileName);
			}
		}

	}
	else
#endif
	{
		printf("Scanning params\n");
		int i=0;
		while (i<argc)
		{
			if(!strcmp(argv[i],"-ase2obj"))
			{
				// i+1 == file or (argc-1) for final argument
				// so argc >= 4
				// argv[1] = -ase2obj
				// argv[2] = inputfile
				// argv[3] = outputfile
				// "D:\Games\quake3\q3ut4\models\null_c\metalbox.ase"
				// "D:\metalbox.obj"
				if(argc >= 4)
				{
					convertAseToObj(argv[2],argv[3]);
					return 0;
				}
				else
				{
					printf("Error: Not enough parameters\n\nUse: -ase2obj infile.ase outfile.obj");
				}
			}
			i++;
		}
	}

	return 0;
}

