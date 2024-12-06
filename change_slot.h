
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

