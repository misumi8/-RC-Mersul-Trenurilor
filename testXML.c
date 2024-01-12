#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

void traverse(xmlNode *node) {
    xmlNode *curNode = NULL;
    for (curNode = node; curNode; curNode = curNode->next) {
        if (curNode->type == XML_ELEMENT_NODE && xmlStrcmp(curNode->name, (const xmlChar *)"statie") == 0) {
            printf("Node: %s\n", curNode->name);

            // Находим дочерние узлы внутри <statie>
            xmlNode *childNode = curNode->children;
            while (childNode != NULL) {
                if (childNode->type == XML_ELEMENT_NODE) {
                    printf("   Child Node: %s = %s\n", childNode->name, xmlNodeGetContent(childNode));
                }
                childNode = childNode->next;
            }
        }
        traverse(curNode->children);
    }
}

struct Statie{
	char arr_time[5]; // xx:xx
	char stat_name[50];
};

/*struct Statii{
	int no_stations;
	struct Statie* opriri;
};*/

struct Tren{
    char id[2];
	char dep_time[5];
	char arr_time[5];
	char from[50];
	char to[50];
	struct Statie* ruta; 
};

int getNoOfStations(xmlNodePtr root){
	xmlNodePtr train = root->children;
	int count = 0;
	while(train != NULL){
		if(xmlStrcmp(train->name, (const xmlChar*)"train") == 0){
            xmlNodePtr awd = train->children;
            while(awd != NULL) {
                if(xmlStrcmp(awd->name, (const xmlChar*)"statie") == 0){
                    count++;
                }
                awd = awd->next;
            }
            printf("%d|", count);
            count = 0;
        }
        train = train->next;
	}
	return count;
}

int main() {
    xmlDocPtr schedule;
	xmlNodePtr root;
	schedule = xmlReadFile("schedule.xml", NULL, 0);
	if(schedule == NULL){
		perror("Eroare la citirea fisierului schedule.xml\n");
	}
	root = xmlDocGetRootElement(schedule);
	if(root == NULL){
		perror("Fisierul xml e gol\n");
    }
	int no_of_trains = getNoOfStations(root);
	//printf("$%d$\n", no_of_trains);
    return 0;
}
