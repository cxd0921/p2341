/*
 * @Author: kunlong.wei@tongdajs.com
 * @Date: 2023-07-19 11:42:53
 * @LastEditors: kunlong.wei@tongdajs.com
 * @LastEditTime: 2023-07-19 14:21:25
 * @FilePath: \wkltest\test_change_slot\src\change_slot.h
 * @Description: 
 * 
 * Copyright (c) 2023 by ${git_name_email}, All Rights Reserved. 
 */
#ifndef CHANGE_SLOT_H
#define CHANGE_SLOT_H


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @description:  切换当前加载分区
 * @return {*} 1 success 0 failed
 */
int api_changeslot();

/**
 * @description: 获取当前slot
 * @return {*} 0:a slot  1: b slot
 */
int api_getCurslot();

#ifdef __cplusplus
}
#endif

#endif

