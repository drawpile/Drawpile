// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/liquify.h>
}
#include "libclient/drawdance/image.h"
#include "libclient/drawdance/liquify.h"
#include <cstring>

namespace drawdance {

Liquify Liquify::init(QRect bounds, const QImage &mask)
{
	FillMaskParams params = {bounds, mask};
	return Liquify(DP_liquify_new(
		bounds.x(), bounds.y(), bounds.width(), bounds.height(),
		&Liquify::fillMask, &params));
}

Liquify Liquify::null()
{
	return Liquify(nullptr);
}

Liquify Liquify::inc(DP_Liquify *l)
{
	return Liquify(DP_liquify_incref_nullable(l));
}

Liquify Liquify::noinc(DP_Liquify *l)
{
	return Liquify(l);
}

Liquify::Liquify()
	: Liquify(nullptr)
{
}

Liquify::Liquify(const Liquify &other)
	: Liquify(DP_liquify_incref_nullable(other.m_data))
{
}

Liquify::Liquify(Liquify &&other)
	: Liquify(other.m_data)
{
	other.m_data = nullptr;
}

Liquify &Liquify::operator=(const Liquify &other)
{
	DP_liquify_decref_nullable(m_data);
	m_data = DP_liquify_incref_nullable(other.m_data);
	return *this;
}

Liquify &Liquify::operator=(Liquify &&other)
{
	DP_liquify_decref_nullable(m_data);
	m_data = other.m_data;
	other.m_data = nullptr;
	return *this;
}

Liquify::~Liquify()
{
	DP_liquify_decref_nullable(m_data);
}

DP_Liquify *Liquify::get() const
{
	return m_data;
}

bool Liquify::isNull() const
{
	return !m_data;
}

LiquifyState Liquify::currentState()
{
	return LiquifyState::noinc(DP_liquify_current_state_inc(m_data));
}

QImage Liquify::dump() const
{
	int width, height;
	uint32_t *data = DP_liquify_dump(m_data, &width, &height);
	return wrapImageUint32(width, height, data);
}

void Liquify::opMove(
	DP_DrawContext *dc, float x, float y, float size, float dx, float dy)
{
	op(dc, int(DP_LIQUIFY_OP_TYPE_MOVE), x, y, size,
	   [dx, dy](DP_LiquifyOpParams &params) {
		   params.op.move.dx = dx;
		   params.op.move.dy = dy;
	   });
}

void Liquify::opScale(
	DP_DrawContext *dc, float x, float y, float size, float amount)
{
	op(dc, int(DP_LIQUIFY_OP_TYPE_SCALE), x, y, size,
	   [amount](DP_LiquifyOpParams &params) {
		   params.op.scale.amount = amount;
	   });
}

void Liquify::opRotate(
	DP_DrawContext *dc, float x, float y, float size, float angle)
{
	op(dc, int(DP_LIQUIFY_OP_TYPE_ROTATE), x, y, size,
	   [angle](DP_LiquifyOpParams &params) {
		   params.op.rotate.angle = angle;
	   });
}

void Liquify::opSmoothe(
	DP_DrawContext *dc, float x, float y, float size, float amount,
	float kernelRadius)
{
	op(dc, int(DP_LIQUIFY_OP_TYPE_SMOOTHE), x, y, size,
	   [amount, kernelRadius](DP_LiquifyOpParams &params) {
		   params.op.smoothe.amount = amount;
		   params.op.smoothe.kernel_radius = kernelRadius;
	   });
}

void Liquify::opErase(
	DP_DrawContext *dc, float x, float y, float size, float amount)
{
	op(dc, int(DP_LIQUIFY_OP_TYPE_ERASE), x, y, size,
	   [amount](DP_LiquifyOpParams &params) {
		   params.op.erase.amount = amount;
	   });
}

Liquify::Liquify(DP_Liquify *l)
	: m_data(l)
{
}

void Liquify::op(
	DP_DrawContext *dc, int type, float x, float y, float size,
	const std::function<void(DP_LiquifyOpParams &params)> &fn)
{
	DP_LiquifyOpParams params;
	params.type = DP_LiquifyOpType(type);
	params.x = x;
	params.y = y;
	params.radius = size * 0.5f;
	fn(params);
	DP_liquify_op(m_data, dc, &params);
}

void Liquify::fillMask(void *user, unsigned char *out)
{
	const FillMaskParams &params = *static_cast<const FillMaskParams *>(user);

	QRect bounds = params.bounds;
	int w = bounds.width();
	int h = bounds.height();

	const QImage &mask = params.mask;
	if(mask.isNull() || mask.size() != bounds.size()) {
		std::memset(out, 255, size_t(w) * size_t(h));
	} else {
		int i = 0;
		for(int y = 0; y < h; ++y) {
			for(int x = 0; x < w; ++x) {
				out[i] = qAlpha(mask.pixel(x, y));
				++i;
			}
		}
	}
}


LiquifyState LiquifyState::null()
{
	return LiquifyState(nullptr);
}

LiquifyState LiquifyState::inc(DP_LiquifyState *ls)
{
	return LiquifyState(DP_liquify_state_incref_nullable(ls));
}

LiquifyState LiquifyState::noinc(DP_LiquifyState *ls)
{
	return LiquifyState(ls);
}

LiquifyState::LiquifyState()
	: LiquifyState(nullptr)
{
}

LiquifyState::LiquifyState(const LiquifyState &other)
	: LiquifyState(DP_liquify_state_incref_nullable(other.m_data))
{
}

LiquifyState::LiquifyState(LiquifyState &&other)
	: LiquifyState(other.m_data)
{
	other.m_data = nullptr;
}

LiquifyState &LiquifyState::operator=(const LiquifyState &other)
{
	DP_liquify_state_decref_nullable(m_data);
	m_data = DP_liquify_state_incref_nullable(other.m_data);
	return *this;
}

LiquifyState &LiquifyState::operator=(LiquifyState &&other)
{
	DP_liquify_state_decref_nullable(m_data);
	m_data = other.m_data;
	other.m_data = nullptr;
	return *this;
}

LiquifyState::~LiquifyState()
{
	DP_liquify_state_decref_nullable(m_data);
}

DP_LiquifyState *LiquifyState::get() const
{
	return m_data;
}

bool LiquifyState::isNull() const
{
	return !m_data;
}

LiquifyState::LiquifyState(DP_LiquifyState *ls)
	: m_data(ls)
{
}


LiquifyTransformer
LiquifyTransformer::init(int sourceX, int sourceY, const QImage &sourceImage)
{
	Q_ASSERT(!sourceImage.isNull());
	Q_ASSERT(sourceImage.format() == QImage::Format_ARGB32_Premultiplied);
	return LiquifyTransformer(DP_liquify_transformer_new(
		sourceX, sourceY, sourceImage.width(), sourceImage.height(),
		reinterpret_cast<const uint32_t *>(sourceImage.constBits())));
}

LiquifyTransformer LiquifyTransformer::null()
{
	return LiquifyTransformer(nullptr);
}

LiquifyTransformer LiquifyTransformer::inc(DP_LiquifyTransformer *lt)
{
	return LiquifyTransformer(DP_liquify_transformer_incref_nullable(lt));
}

LiquifyTransformer LiquifyTransformer::noinc(DP_LiquifyTransformer *lt)
{
	return LiquifyTransformer(lt);
}

LiquifyTransformer::LiquifyTransformer()
	: LiquifyTransformer(nullptr)
{
}

LiquifyTransformer::LiquifyTransformer(const LiquifyTransformer &other)
	: LiquifyTransformer(DP_liquify_transformer_incref_nullable(other.m_data))
{
}

LiquifyTransformer::LiquifyTransformer(LiquifyTransformer &&other)
	: LiquifyTransformer(other.m_data)
{
	other.m_data = nullptr;
}

LiquifyTransformer &
LiquifyTransformer::operator=(const LiquifyTransformer &other)
{
	DP_liquify_transformer_decref_nullable(m_data);
	m_data = DP_liquify_transformer_incref_nullable(other.m_data);
	return *this;
}

LiquifyTransformer &LiquifyTransformer::operator=(LiquifyTransformer &&other)
{
	DP_liquify_transformer_decref_nullable(m_data);
	m_data = other.m_data;
	other.m_data = nullptr;
	return *this;
}

LiquifyTransformer::~LiquifyTransformer()
{
	DP_liquify_transformer_decref_nullable(m_data);
}

DP_LiquifyTransformer *LiquifyTransformer::get() const
{
	return m_data;
}

bool LiquifyTransformer::isNull() const
{
	return !m_data;
}

bool LiquifyTransformer::apply(
	const LiquifyState &liquifyState, int interpolation)
{
	return DP_liquify_transformer_apply(
		m_data, liquifyState.get(), interpolation);
}

QImage LiquifyTransformer::targetImage(QPoint &outPos) const
{
	int x, y, width, height;
	const uint32_t *data;
	bool have_image = DP_liquify_transformer_target_image(
		m_data, &x, &y, &width, &height, &data);

	if(have_image) {
		QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
		size_t stride = size_t(width) * sizeof(uint32_t);
		if(size_t(img.bytesPerLine()) == stride) {
			std::memcpy(img.bits(), data, stride * size_t(height));
		} else {
			for(int i = 0; i < height; ++i) {
				std::memcpy(img.scanLine(i), data, stride);
				data += width;
			}
		}
		outPos = QPoint(x, y);
		return img;
	} else {
		return QImage();
	}
}

LiquifyTransformer::LiquifyTransformer(DP_LiquifyTransformer *lt)
	: m_data(lt)
{
}

void LiquifyTransformer::disposeImage(void *user)
{
	QImage *img = static_cast<QImage *>(user);
	delete img;
}

}
