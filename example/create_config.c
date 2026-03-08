#define SEIN_IMPLEMENTATION
#include "../sein.h"

int main(void){
    SeinDocument *doc = sein_doc_alloc("./configC.sein");
    
    sein_doc_add_comment(doc, "Auto-generated");
    sein_doc_add_include(doc, "colors.sein", NULL);
    sein_doc_add_global_var(doc, "APP_NAME", "My App", NULL);
    sein_doc_add_section(doc, "Window", NULL);
    sein_doc_add_value(doc, "Window", "Title", "${APP_NAME}", NULL);
    sein_doc_add_value(doc, "Window", "Width", "1280", "px");
    sein_doc_save(doc);
    sein_doc_free(doc);
}