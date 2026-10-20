#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H
typedef void (*button_manager_callback_t)(void);
int button_manager_init(button_manager_callback_t single, button_manager_callback_t double_press);
#endif
