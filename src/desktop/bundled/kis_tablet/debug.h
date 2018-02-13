#define dbgTablet qDebug()
#define dbgInput qDebug()

#define KIS_SAFE_ASSERT_RECOVER(cond) if (!(cond))
#define KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(cond, val) KIS_SAFE_ASSERT_RECOVER(cond) { return (val); }

