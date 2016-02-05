#define	CMINORS	64
#define	BUFSIZ	10240
typedef struct{
	unsigned int wpos;//次に書き込む位置
	unsigned int rpos;//次に読み込む位置
	unsigned char	buf[BUFSIZ];
}BUFFER;
/*
読み出し
	wpos==rposのときには読み込むデータがありません
	wpos!=rposのときwpos-1の位置まで読み出し可能
	読んだらrpos=wposとする（最後に読んだ場所の次に置く）
書き込み
	wpos==rpos-1であればバッファフルで書き込み不能
	wpos!=rpos-1であれば、rpos-1まで書き込み可能
	書き終わったら、wposを最後に書き込んだ場所の次に置く
*/


extern void exit_ctl(void);
extern void exit_chtty(void);
extern int  init_ctl(void);
extern int  init_chtty(void);

extern int create_htty(char *name,int index,BUFFER *r,BUFFER *w);
extern void delete_htty(int index);
