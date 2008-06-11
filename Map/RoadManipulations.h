#ifndef MERKAARTOR_ROADMANIPULATIONS_H_
#define MERKAARTOR_ROADMANIPULATIONS_H_

class CommandList;
class MapDocument;
class MapLayer;
class PropertiesDock;
class Road;

#include <vector>

void joinRoads(MapDocument* theDocument, CommandList* theList, PropertiesDock* theDock);
void splitRoads(MapLayer* theLayer, CommandList* theList, PropertiesDock* theDock);
void breakRoads(MapLayer* theLayer, CommandList* theList, PropertiesDock* theDock);
void reversePoints(MapLayer* theLayer, CommandList* theList, Road* R);
void alignNodes(MapLayer* theLayer, CommandList* theList, PropertiesDock* theDock);
void mergeNodes(MapDocument* theDocument, CommandList* theList, PropertiesDock* theDock);

#endif

