#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

int main() {
    xmlDocPtr doc;
    xmlNodePtr rootNode, node;

    // Открытие XML файла
    doc = xmlReadFile("schedule.xml", NULL, 0);

    if (doc == NULL) {
        fprintf(stderr, "Ошибка чтения файла XML.\n");
        return 1;
    }

    // Получение корневого элемента
    rootNode = xmlDocGetRootElement(doc);

    if (rootNode == NULL) {
        fprintf(stderr, "Пустой документ XML.\n");
        xmlFreeDoc(doc);
        return 1;
    }

    // Перебор элементов внутри корневого элемента
    for (node = rootNode->children; node; node = node->next) {
        // Обработка элементов (здесь вывод на экран)
        printf("Имя элемента: %s\n", node->name);

        // Если есть атрибуты у элемента, можно получить доступ к ним
        xmlAttr *attr = node->properties;
        while (attr != NULL) {
            printf("Атрибут: %s = %s\n", attr->name, attr->children->content);
            attr = attr->next;
        }
    }

    // Освобождение ресурсов
    xmlFreeDoc(doc);
    xmlCleanupParser();

    return 0;
}
