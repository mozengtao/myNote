/*
 * strategy_pattern_ops.c
 *
 * 场景：策略模式（Strategy Pattern）—— 一个"压缩管道"，压缩算法本身
 * 可以在运行期切换（none / RLE / XOR-mask），管道核心代码永远不变。
 *
 * 这与 Kernel 里 struct crypto_alg / 调度器的 struct sched_class（08 篇后续批次）
 * 是同一种范式：**核心逻辑不变，策略（algorithm）通过 ops table 注入替换**。
 *
 *   pipeline_process()
 *        │
 *        ▼
 *   codec->ops->compress(codec, in, out)     <-- 策略注入点
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o strategy_pattern_ops strategy_pattern_ops.c
 * 运行：
 *   ./strategy_pattern_ops
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SZ 256

struct codec;

/* ------------------------------------------------------------------ */
/* 1. 契约：codec_ops —— 策略接口                                      */
/* ------------------------------------------------------------------ */

struct codec_ops {
	const char *algo_name;
	/* 返回写入 out 的字节数 */
	int (*compress)(struct codec *self, const unsigned char *in, int in_len, unsigned char *out);
	int (*decompress)(struct codec *self, const unsigned char *in, int in_len, unsigned char *out);
};

struct codec {
	const struct codec_ops *ops;   /* <-- 策略注入点 */
	long bytes_in;
	long bytes_out;
};

/* ------------------------------------------------------------------ */
/* 2. Core：管道逻辑，永远不知道具体是哪种算法                          */
/* ------------------------------------------------------------------ */

static int codec_compress(struct codec *c, const unsigned char *in, int in_len, unsigned char *out)
{
	int n = c->ops->compress(c, in, in_len, out);
	c->bytes_in += in_len;
	c->bytes_out += n;
	return n;
}

static int codec_decompress(struct codec *c, const unsigned char *in, int in_len, unsigned char *out)
{
	return c->ops->decompress(c, in, in_len, out);
}

static void pipeline_run(struct codec *c, const char *label, const unsigned char *msg, int len)
{
	unsigned char compressed[BUF_SZ];
	unsigned char restored[BUF_SZ];
	int clen, rlen;
	int ok;

	printf("-- pipeline[%s] using algo='%s' --\n", label, c->ops->algo_name);

	clen = codec_compress(c, msg, len, compressed);
	printf("  compress: %d bytes -> %d bytes (ratio %.2f)\n",
	       len, clen, len ? (double)clen / len : 0.0);

	rlen = codec_decompress(c, compressed, clen, restored);
	ok = (rlen == len) && (memcmp(restored, msg, (size_t)len) == 0);
	printf("  decompress: %d bytes -> %d bytes, round-trip %s\n",
	       clen, rlen, ok ? "OK" : "MISMATCH");
}

/* ------------------------------------------------------------------ */
/* 3. 策略 A：none —— 什么都不做，原样拷贝（基线对照）                  */
/* ------------------------------------------------------------------ */

static int none_compress(struct codec *self, const unsigned char *in, int in_len, unsigned char *out)
{
	(void)self;
	memcpy(out, in, (size_t)in_len);
	return in_len;
}

static int none_decompress(struct codec *self, const unsigned char *in, int in_len, unsigned char *out)
{
	(void)self;
	memcpy(out, in, (size_t)in_len);
	return in_len;
}

static const struct codec_ops none_ops = {
	.algo_name   = "none",
	.compress    = none_compress,
	.decompress  = none_decompress,
};

/* ------------------------------------------------------------------ */
/* 4. 策略 B：RLE（Run-Length Encoding）—— 编码格式: [count][byte] ...  */
/* ------------------------------------------------------------------ */

static int rle_compress(struct codec *self, const unsigned char *in, int in_len, unsigned char *out)
{
	(void)self;
	int i = 0, o = 0;
	while (i < in_len) {
		unsigned char b = in[i];
		int run = 1;
		while (i + run < in_len && in[i + run] == b && run < 255)
			run++;
		out[o++] = (unsigned char)run;
		out[o++] = b;
		i += run;
	}
	return o;
}

static int rle_decompress(struct codec *self, const unsigned char *in, int in_len, unsigned char *out)
{
	(void)self;
	int i = 0, o = 0;
	while (i < in_len) {
		unsigned char run = in[i];
		unsigned char b = in[i + 1];
		int k;
		for (k = 0; k < run; k++)
			out[o++] = b;
		i += 2;
	}
	return o;
}

static const struct codec_ops rle_ops = {
	.algo_name   = "rle",
	.compress    = rle_compress,
	.decompress  = rle_decompress,
};

/* ------------------------------------------------------------------ */
/* 5. 策略 C：xor-mask —— 简单"加密”（异或固定 key），演示策略也可以    */
/*    不是"压缩"语义，只要满足 codec_ops 契约就能插进同一个管道          */
/* ------------------------------------------------------------------ */

#define XOR_KEY 0x5A

static int xor_compress(struct codec *self, const unsigned char *in, int in_len, unsigned char *out)
{
	(void)self;
	int i;
	for (i = 0; i < in_len; i++)
		out[i] = in[i] ^ XOR_KEY;
	return in_len;
}

static int xor_decompress(struct codec *self, const unsigned char *in, int in_len, unsigned char *out)
{
	return xor_compress(self, in, in_len, out);   /* XOR 是自逆操作 */
}

static const struct codec_ops xor_ops = {
	.algo_name   = "xor-mask",
	.compress    = xor_compress,
	.decompress  = xor_decompress,
};

int main(void)
{
	const unsigned char msg1[] = "aaaaaaaabbbbccccccccccccdddd";
	const unsigned char msg2[] = "The quick brown fox jumps over the lazy dog";
	struct codec c;

	printf("=== strategy_pattern_ops demo ===\n\n");

	/* 同一个 codec 对象，运行期切换策略 —— 这才是"策略模式"的精髓 */
	c = (struct codec){ .ops = &none_ops, .bytes_in = 0, .bytes_out = 0 };
	pipeline_run(&c, "A", msg1, (int)strlen((const char *)msg1));

	c = (struct codec){ .ops = &rle_ops, .bytes_in = 0, .bytes_out = 0 };
	pipeline_run(&c, "B", msg1, (int)strlen((const char *)msg1));

	c = (struct codec){ .ops = &rle_ops, .bytes_in = 0, .bytes_out = 0 };
	pipeline_run(&c, "C", msg2, (int)strlen((const char *)msg2));

	c = (struct codec){ .ops = &xor_ops, .bytes_in = 0, .bytes_out = 0 };
	pipeline_run(&c, "D", msg2, (int)strlen((const char *)msg2));

	printf("\n[core] final codec stats: bytes_in=%ld bytes_out=%ld (algo=%s)\n",
	       c.bytes_in, c.bytes_out, c.ops->algo_name);

	printf("\n=== done ===\n");
	return 0;
}
