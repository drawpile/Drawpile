// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_LIQUIFY_H
#define LIBCLIENT_DRAWDANCE_LIQUIFY_H
#include <QImage>
#include <QRect>
#include <functional>

struct DP_DrawContext;
struct DP_Liquify;
struct DP_LiquifyOpParams;
struct DP_LiquifyState;
struct DP_LiquifyTransformer;

namespace drawdance {

class LiquifyState;

class Liquify final {
public:
	static Liquify init(QRect bounds, const QImage &mask);
	static Liquify null();
	static Liquify inc(DP_Liquify *l);
	static Liquify noinc(DP_Liquify *l);

	Liquify();
	Liquify(const Liquify &other);
	Liquify(Liquify &&other);
	Liquify &operator=(const Liquify &other);
	Liquify &operator=(Liquify &&other);
	~Liquify();

	DP_Liquify *get() const;

	bool isNull() const;

	LiquifyState currentState();

	QImage dump() const;

	void opMove(
		DP_DrawContext *dc, float x, float y, float size, float dx, float dy);

	void
	opScale(DP_DrawContext *dc, float x, float y, float size, float amount);

	void
	opRotate(DP_DrawContext *dc, float x, float y, float size, float angle);

	void opSmoothe(
		DP_DrawContext *dc, float x, float y, float size, float amount,
		float kernelRadius);

	void
	opErase(DP_DrawContext *dc, float x, float y, float size, float amount);

private:
	struct FillMaskParams {
		QRect bounds;
		const QImage &mask;
	};

	explicit Liquify(DP_Liquify *l);

	void
	op(DP_DrawContext *dc, int type, float x, float y, float size,
	   const std::function<void(DP_LiquifyOpParams &params)> &fn);

	static void fillMask(void *user, unsigned char *out);

	DP_Liquify *m_data;
};


class LiquifyState final {
public:
	static LiquifyState null();
	static LiquifyState inc(DP_LiquifyState *ls);
	static LiquifyState noinc(DP_LiquifyState *ls);

	LiquifyState();
	LiquifyState(const LiquifyState &other);
	LiquifyState(LiquifyState &&other);
	LiquifyState &operator=(const LiquifyState &other);
	LiquifyState &operator=(LiquifyState &&other);
	~LiquifyState();

	DP_LiquifyState *get() const;

	bool isNull() const;

private:
	explicit LiquifyState(DP_LiquifyState *ls);

	DP_LiquifyState *m_data;
};


class LiquifyTransformer final {
public:
	static LiquifyTransformer
	init(int sourceX, int sourceY, const QImage &sourceImage);
	static LiquifyTransformer null();
	static LiquifyTransformer inc(DP_LiquifyTransformer *lt);
	static LiquifyTransformer noinc(DP_LiquifyTransformer *lt);

	LiquifyTransformer();
	LiquifyTransformer(const LiquifyTransformer &other);
	LiquifyTransformer(LiquifyTransformer &&other);
	LiquifyTransformer &operator=(const LiquifyTransformer &other);
	LiquifyTransformer &operator=(LiquifyTransformer &&other);
	~LiquifyTransformer();

	DP_LiquifyTransformer *get() const;

	bool isNull() const;

	bool apply(const LiquifyState &liquifyState, int interpolation);

	QImage targetImage(QPoint &outPos) const;

private:
	explicit LiquifyTransformer(DP_LiquifyTransformer *lt);

	static void disposeImage(void *user);

	DP_LiquifyTransformer *m_data;
};

}

#endif
