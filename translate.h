//
// Created by whz on 15-11-18.
//

#ifndef __TRANSLATE_H__
#define __TRANSLATE_H__

// 标志翻译状态, 决策最终代码生成, 状态可叠加
typedef enum {
    FINE = 0,
    ERROR = (1 << 0),
    UNSUPPORT = (1 << 1),
} TranslateState;

#endif // __TRANSLATE_H__
