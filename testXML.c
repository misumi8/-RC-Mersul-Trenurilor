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

int main() {
    xmlDoc *doc = NULL;
    xmlNode *root = NULL;

    doc = xmlReadFile("schedule.xml", NULL, 0);
    if (doc == NULL) {
        printf("Error parsing XML file.\n");
        return 1;
    }

    root = xmlDocGetRootElement(doc);

    traverse(root);

    xmlFreeDoc(doc);
    xmlCleanupParser();

    return 0;
}
