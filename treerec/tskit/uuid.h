#ifndef TSK_UUID_H
#define TSK_UUID_H

#ifdef __cplusplus
extern "C" {
#endif

#define TSK_UUID_SIZE 36

int tsk_generate_uuid(char *dest, int flags);

#ifdef __cplusplus
}
#endif
#endif /* TSK_UUID_H */
