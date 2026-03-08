#include "../sein.hpp"

int main(void){
    auto doc = fd::sein::create_new_config("./configCPP.sein");

    fd::sein::add_header_comment(doc, "Auto-generated");
    fd::sein::add_include(doc, "colors.sein");
    fd::sein::add_global_var(doc, "APP_NAME", "My App");
    fd::sein::add_section(doc, "Window");
    fd::sein::add_value(doc, "Window", "Title", "${APP_NAME}");
    fd::sein::add_value(doc, "Window", "Width", "1280", "px");
    fd::sein::save_config(doc);
}