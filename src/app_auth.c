/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file app_auth.c
 * @brief CMD 0x03 认证与密码管理模块。
 *
 * 测试阶段默认密码: 123456
 * password_hash = SHA256(password + device_sn + salt)
 * salt = "ULP_RS100_AUTH_V1"
 * token = HMAC_SHA256(password_hash, nonce + device_sn)
 *
 * SHA256 与 HMAC-SHA256 为自包含实现，不依赖外部密码库。
 *
 * 【密码持久化策略】
 * 本测试工程暂不将 password_hash 写入 Flash，修改密码仅保存在 RAM 中。
 * 设备重启后 password_hash 恢复为默认密码 "123456" 的计算值。
 * 如需完整测试修改密码，应使用 Zephyr settings 或 Flash KV 持久化
 * password_hash，在 app_auth_init() 中先从持久化存储加载，不存在时
 * 再 fallback 到默认密码。
 */

#include "app_auth.h"
#include "app_config.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/random/random.h>

#include <string.h>

/* ── 设备身份 ── */

static const char g_sn[]   = "123456";      /* 与 app_config.c 保持一致 */
static const char g_salt[] = "ULP_RS100_AUTH_V1";

/* ── 默认测试密码 ── */

#define DEFAULT_PASSWORD "123456"

/* ── 密码哈希（32 字节 SHA256，设备端永不保留明文） ── */

static uint8_t g_password_hash[32];

/* ── BLE / NFC 独立 nonce 状态，避免两个通道互相覆盖 ── */

static uint8_t g_nonce_ble[8];
static bool    g_nonce_ble_valid;
static uint8_t g_nonce_nfc[8];
static bool    g_nonce_nfc_valid;

/* ── 按通道取 nonce 指针 ── */

static uint8_t *channel_nonce(const char *channel)
{
	return (channel[0] == 'B') ? g_nonce_ble : g_nonce_nfc;
}

static bool *channel_nonce_valid(const char *channel)
{
	return (channel[0] == 'B') ? &g_nonce_ble_valid : &g_nonce_nfc_valid;
}

/* ── 会话解锁状态 ── */

static bool     g_ble_unlocked;
static int64_t  g_nfc_auth_until_ms;  /* k_uptime_get() + 5*60*1000，0 = 锁定 */
#define NFC_AUTH_WINDOW_MS  (5 * 60 * 1000)

/* ═══════════════════════════════════════════════════════════════════════
 * 自包含 SHA256 实现 (FIPS 180-4)
 * ═══════════════════════════════════════════════════════════════════════ */

#define SHA256_BLOCK_SIZE 64
#define SHA256_HASH_SIZE  32

struct sha256_ctx {
	uint32_t state[8];
	uint64_t count;
	uint8_t  buf[SHA256_BLOCK_SIZE];
};

static const uint32_t sha256_k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t sha256_rotr(uint32_t x, uint32_t n)
{
	return (x >> n) | (x << (32 - n));
}

static inline uint32_t sha256_ch(uint32_t x, uint32_t y, uint32_t z)
{
	return (x & y) ^ (~x & z);
}

static inline uint32_t sha256_maj(uint32_t x, uint32_t y, uint32_t z)
{
	return (x & y) ^ (x & z) ^ (y & z);
}

static inline uint32_t sha256_bsig0(uint32_t x)
{
	return sha256_rotr(x, 2) ^ sha256_rotr(x, 13) ^ sha256_rotr(x, 22);
}

static inline uint32_t sha256_bsig1(uint32_t x)
{
	return sha256_rotr(x, 6) ^ sha256_rotr(x, 11) ^ sha256_rotr(x, 25);
}

static inline uint32_t sha256_ssig0(uint32_t x)
{
	return sha256_rotr(x, 7) ^ sha256_rotr(x, 18) ^ (x >> 3);
}

static inline uint32_t sha256_ssig1(uint32_t x)
{
	return sha256_rotr(x, 17) ^ sha256_rotr(x, 19) ^ (x >> 10);
}

static inline uint32_t sha256_get32be(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static inline void sha256_put32be(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)(v);
}

static void sha256_transform(uint32_t *state, const uint8_t *block)
{
	uint32_t w[64];
	uint32_t a, b, c, d, e, f, g, h;
	uint32_t t1, t2;

	for (int i = 0; i < 16; i++) {
		w[i] = sha256_get32be(&block[i * 4]);
	}
	for (int i = 16; i < 64; i++) {
		w[i] = sha256_ssig1(w[i - 2]) + w[i - 7] +
		       sha256_ssig0(w[i - 15]) + w[i - 16];
	}

	a = state[0]; b = state[1]; c = state[2]; d = state[3];
	e = state[4]; f = state[5]; g = state[6]; h = state[7];

	for (int i = 0; i < 64; i++) {
		t1 = h + sha256_bsig1(e) + sha256_ch(e, f, g) + sha256_k[i] + w[i];
		t2 = sha256_bsig0(a) + sha256_maj(a, b, c);
		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}

	state[0] += a; state[1] += b; state[2] += c; state[3] += d;
	state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static void sha256_init(struct sha256_ctx *ctx)
{
	ctx->state[0] = 0x6a09e667;
	ctx->state[1] = 0xbb67ae85;
	ctx->state[2] = 0x3c6ef372;
	ctx->state[3] = 0xa54ff53a;
	ctx->state[4] = 0x510e527f;
	ctx->state[5] = 0x9b05688c;
	ctx->state[6] = 0x1f83d9ab;
	ctx->state[7] = 0x5be0cd19;
	ctx->count = 0;
}

static void sha256_update(struct sha256_ctx *ctx,
			  const uint8_t *data, size_t len)
{
	size_t idx = (size_t)(ctx->count & 0x3F); /* ctx->count % 64 */
	ctx->count += (uint64_t)len;

	while (len > 0) {
		size_t fill = SHA256_BLOCK_SIZE - idx;
		if (fill > len) {
			fill = len;
		}
		memcpy(ctx->buf + idx, data, fill);
		idx += fill;
		data += fill;
		len -= fill;

		if (idx == SHA256_BLOCK_SIZE) {
			sha256_transform(ctx->state, ctx->buf);
			idx = 0;
		}
	}
}

static void sha256_finish(struct sha256_ctx *ctx, uint8_t *hash)
{
	uint64_t bits = ctx->count * 8;
	size_t idx = (size_t)(ctx->count & 0x3F);

	/* padding: 0x80, then zeros, then 8-byte big-endian bit count */
	ctx->buf[idx++] = 0x80;
	if (idx > 56) {
		memset(ctx->buf + idx, 0, SHA256_BLOCK_SIZE - idx);
		sha256_transform(ctx->state, ctx->buf);
		idx = 0;
	}
	memset(ctx->buf + idx, 0, 56 - idx);
	sha256_put32be(ctx->buf + 56, (uint32_t)(bits >> 32));
	sha256_put32be(ctx->buf + 60, (uint32_t)(bits));
	sha256_transform(ctx->state, ctx->buf);

	for (int i = 0; i < 8; i++) {
		sha256_put32be(hash + i * 4, ctx->state[i]);
	}
}

/* ═══════════════════════════════════════════════════════════════════════
 * 自包含 HMAC-SHA256 实现 (RFC 2104)
 * ═══════════════════════════════════════════════════════════════════════ */

static void hmac_sha256(const uint8_t *key, size_t key_len,
			const uint8_t *msg, size_t msg_len,
			uint8_t *hmac_out)
{
	uint8_t key_pad[SHA256_BLOCK_SIZE];
	struct sha256_ctx ctx;
	uint8_t inner_hash[SHA256_HASH_SIZE];

	/* 如果 key 长于 block size，先 hash */
	if (key_len > SHA256_BLOCK_SIZE) {
		sha256_init(&ctx);
		sha256_update(&ctx, key, key_len);
		sha256_finish(&ctx, key_pad);
		memset(key_pad + SHA256_HASH_SIZE, 0,
		       SHA256_BLOCK_SIZE - SHA256_HASH_SIZE);
	} else {
		memcpy(key_pad, key, key_len);
		if (key_len < SHA256_BLOCK_SIZE) {
			memset(key_pad + key_len, 0,
			       SHA256_BLOCK_SIZE - key_len);
		}
	}

	/* inner: H((key_pad ^ ipad) || msg) */
	uint8_t inner_key[SHA256_BLOCK_SIZE];
	for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
		inner_key[i] = key_pad[i] ^ 0x36;
	}
	sha256_init(&ctx);
	sha256_update(&ctx, inner_key, SHA256_BLOCK_SIZE);
	sha256_update(&ctx, msg, msg_len);
	sha256_finish(&ctx, inner_hash);

	/* outer: H((key_pad ^ opad) || inner_hash) */
	uint8_t outer_key[SHA256_BLOCK_SIZE];
	for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
		outer_key[i] = key_pad[i] ^ 0x5C;
	}
	sha256_init(&ctx);
	sha256_update(&ctx, outer_key, SHA256_BLOCK_SIZE);
	sha256_update(&ctx, inner_hash, SHA256_HASH_SIZE);
	sha256_finish(&ctx, hmac_out);
}

/* ═══════════════════════════════════════════════════════════════════════
 * 密码哈希辅助
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief 计算密码哈希。
 *
 *   password_hash = SHA256(password || device_sn || salt)
 */
static void compute_password_hash(const char *password,
				  const char *sn,
				  const char *salt,
				  uint8_t *hash_out)
{
	struct sha256_ctx ctx;

	sha256_init(&ctx);
	sha256_update(&ctx, (const uint8_t *)password, strlen(password));
	sha256_update(&ctx, (const uint8_t *)sn,       strlen(sn));
	sha256_update(&ctx, (const uint8_t *)salt,      strlen(salt));
	sha256_finish(&ctx, hash_out);
}

/* ═══════════════════════════════════════════════════════════════════════
 * 会话管理
 * ═══════════════════════════════════════════════════════════════════════ */

static void channel_lock(const char *channel)
{
	if (channel[0] == 'B') {
		g_ble_unlocked = false;
		*channel_nonce_valid(channel) = false;
		printk("[AUTH] BLE locked\n");
	} else {
		g_nfc_auth_until_ms = 0;
		*channel_nonce_valid(channel) = false;
		printk("[AUTH] NFC locked\n");
	}
}

static void channel_unlock(const char *channel)
{
	if (channel[0] == 'B') {
		g_ble_unlocked = true;
		printk("[AUTH] BLE unlocked (this connection)\n");
	} else {
		g_nfc_auth_until_ms = k_uptime_get() + NFC_AUTH_WINDOW_MS;
		printk("[AUTH] NFC unlocked for %u s\n",
		       (unsigned)(NFC_AUTH_WINDOW_MS / 1000));
	}
}

/* ═══════════════════════════════════════════════════════════════════════
 * 公共 API
 * ═══════════════════════════════════════════════════════════════════════ */

void app_auth_init(void)
{
	compute_password_hash(DEFAULT_PASSWORD, g_sn, g_salt, g_password_hash);
	printk("[AUTH] init OK (default password, RAM-only, lost on reboot)\n");
}

uint16_t app_auth_handle_cmd(const uint8_t *data, uint16_t len,
			     uint8_t *resp, const char *channel)
{
	if (len < 2 || data[0] != APP_CONFIG_CMD_AUTH) {
		return 0;
	}

	uint8_t subcmd = data[1];
	uint8_t *nonce = channel_nonce(channel);
	bool *nonce_valid = channel_nonce_valid(channel);

	switch (subcmd) {

	/* ── 0x01: 请求 nonce ── */
	case APP_CONFIG_AUTH_SUBCMD_NONCE: {
		sys_rand_get(nonce, 8);
		*nonce_valid = true;

		resp[0] = APP_CONFIG_CMD_AUTH;
		resp[1] = APP_CONFIG_STATUS_OK;
		resp[2] = APP_CONFIG_AUTH_SUBCMD_NONCE;
		memcpy(&resp[3], nonce, 8);

		printk("[AUTH] %s nonce issued\n", channel);
		return 11;
	}

	/* ── 0x02: 提交认证 token ── */
	case APP_CONFIG_AUTH_SUBCMD_TOKEN: {
		if (len < 34) {
			resp[0] = APP_CONFIG_CMD_AUTH;
			resp[1] = APP_CONFIG_STATUS_PARAM_LEN;
			resp[2] = APP_CONFIG_AUTH_SUBCMD_TOKEN;
			return 3;
		}

		if (!*nonce_valid) {
			resp[0] = APP_CONFIG_CMD_AUTH;
			resp[1] = APP_CONFIG_STATUS_AUTH_STATE;
			resp[2] = APP_CONFIG_AUTH_SUBCMD_TOKEN;
			resp[3] = APP_CONFIG_AUTH_STATE_LOCKED;
			printk("[AUTH] %s token without nonce\n", channel);
			return 4;
		}

		/* 构造 HMAC 消息: nonce(8) || sn */
		uint8_t hmac_msg[256];
		size_t sn_len = strlen(g_sn);
		memcpy(hmac_msg, nonce, 8);
		memcpy(hmac_msg + 8, g_sn, sn_len);

		/* 计算期望 token */
		uint8_t expected[32];
		hmac_sha256(g_password_hash, 32,
			    hmac_msg, 8 + sn_len,
			    expected);

		/* 真正常量时间比较：始终遍历全部 32 字节，无提前退出 */
		const uint8_t *received = &data[2];
		uint8_t diff = 0;
		for (int i = 0; i < 32; i++) {
			diff |= (expected[i] ^ received[i]);
		}
		bool match = (diff == 0);

		/* 无论成功与否都消费 nonce（一次性使用） */
		*nonce_valid = false;

		resp[0] = APP_CONFIG_CMD_AUTH;

		if (match) {
			channel_unlock(channel);
			resp[1] = APP_CONFIG_STATUS_OK;
			resp[2] = APP_CONFIG_AUTH_SUBCMD_TOKEN;
			resp[3] = APP_CONFIG_AUTH_STATE_UNLOCKED;
			printk("[AUTH] %s token OK\n", channel);
		} else {
			channel_lock(channel);
			resp[1] = APP_CONFIG_STATUS_AUTH_FAILED;
			resp[2] = APP_CONFIG_AUTH_SUBCMD_TOKEN;
			resp[3] = APP_CONFIG_AUTH_STATE_LOCKED;
			printk("[AUTH] %s token mismatch\n", channel);
		}
		return 4;
	}

	/* ── 0x03: 修改密码 ── */
	case APP_CONFIG_AUTH_SUBCMD_CHANGE_PWD: {
		uint8_t st = app_auth_check_write(channel);
		if (st != APP_CONFIG_STATUS_OK) {
			resp[0] = APP_CONFIG_CMD_AUTH;
			resp[1] = st;
			resp[2] = APP_CONFIG_AUTH_SUBCMD_CHANGE_PWD;
			return 3;
		}

		if (len < 34) {
			resp[0] = APP_CONFIG_CMD_AUTH;
			resp[1] = APP_CONFIG_STATUS_PARAM_LEN;
			resp[2] = APP_CONFIG_AUTH_SUBCMD_CHANGE_PWD;
			return 3;
		}

		/* 保存新密码 hash（32 字节，仅 RAM，重启丢失恢复默认） */
		memcpy(g_password_hash, &data[2], 32);

		resp[0] = APP_CONFIG_CMD_AUTH;
		resp[1] = APP_CONFIG_STATUS_OK;
		resp[2] = APP_CONFIG_AUTH_SUBCMD_CHANGE_PWD;

		printk("[AUTH] %s password changed (RAM-only, lost on reboot)\n",
		       channel);
		return 3;
	}

	/* ── 0x04: 主动锁定 ── */
	case APP_CONFIG_AUTH_SUBCMD_LOCK: {
		channel_lock(channel);

		resp[0] = APP_CONFIG_CMD_AUTH;
		resp[1] = APP_CONFIG_STATUS_OK;
		resp[2] = APP_CONFIG_AUTH_SUBCMD_LOCK;
		return 3;
	}

	default:
		resp[0] = APP_CONFIG_CMD_AUTH;
		resp[1] = APP_CONFIG_STATUS_UNSUPPORTED;
		resp[2] = subcmd;
		return 3;
	}
}

uint8_t app_auth_check_write(const char *channel)
{
	if (channel[0] == 'B') {
		if (g_ble_unlocked) {
			return APP_CONFIG_STATUS_OK;
		}
		return APP_CONFIG_STATUS_AUTH_REQUIRED;
	}

	/* NFC: 惰性检查时效 */
	if (g_nfc_auth_until_ms > 0) {
		if ((int64_t)k_uptime_get() <= g_nfc_auth_until_ms) {
			return APP_CONFIG_STATUS_OK;
		}
		g_nfc_auth_until_ms = 0;
		return APP_CONFIG_STATUS_AUTH_EXPIRED;
	}
	return APP_CONFIG_STATUS_AUTH_REQUIRED;
}

void app_auth_ble_disconnect(void)
{
	g_ble_unlocked = false;
	g_nonce_ble_valid = false;
	printk("[AUTH] BLE disconnected, lock & nonce cleared\n");
}
