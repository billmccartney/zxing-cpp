/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2017 Axel Waggershauser
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "GenericGFPoly.h"

#include "GenericGF.h"
#include "ZXConfig.h"
#include "ZXContainerAlgorithms.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace ZXing {

int
GenericGFPoly::evaluateAt(int a) const
{
	if (a == 0)
		// Just return the x^0 coefficient
		return coefficient(0);

	if (a == 1)
		// Just the sum of the coefficients
		return Reduce(_coefficients, 0, [](auto a, auto b) { return a ^ b; });

	int result = _coefficients[0];
	for (size_t i = 1; i < _coefficients.size(); ++i)
		result = _field->multiply(a, result) ^ _coefficients[i];
	return result;
}

GenericGFPoly& GenericGFPoly::addOrSubtract(GenericGFPoly& other)
{
	assert(_field == other._field); // "GenericGFPolys do not have same GenericGF field"
	
	if (isZero()) {
		swap(*this, other);
		return *this;
	}
	
	if (other.isZero())
		return *this;

	auto& smallerCoefs = other._coefficients;
	auto& largerCoefs = _coefficients;
	if (smallerCoefs.size() > largerCoefs.size())
		std::swap(smallerCoefs, largerCoefs);

	size_t lengthDiff = largerCoefs.size() - smallerCoefs.size();

	// high-order terms only found in higher-degree polynomial's coefficients stay untouched
	for (size_t i = lengthDiff; i < largerCoefs.size(); ++i)
		largerCoefs[i] ^= smallerCoefs[i - lengthDiff];

	normalize();
	return *this;
}

GenericGFPoly&
GenericGFPoly::multiply(const GenericGFPoly& other)
{
	assert(_field == other._field); // "GenericGFPolys do not have same GenericGF field"

	if (isZero() || other.isZero())
		return setMonomial(0);

	auto& a = _coefficients;
	auto& b = other._coefficients;

	// To disable the use of the malloc cache, simply uncomment:
	// Coefficients _cache;
	_cache.resize(a.size() + b.size() - 1);
	std::fill(_cache.begin(), _cache.end(), 0);
	for (size_t i = 0; i < a.size(); ++i)
		for (size_t j = 0; j < b.size(); ++j)
			_cache[i + j] ^= _field->multiply(a[i], b[j]);

	_coefficients.swap(_cache);

	normalize();
	return *this;
}

GenericGFPoly&
GenericGFPoly::multiplyByMonomial(int degree, int coefficient)
{
	assert(degree >= 0);

	if (coefficient == 0)
		return setMonomial(0);

	for (int& c : _coefficients)
		c = _field->multiply(c, coefficient);

	_coefficients.resize(_coefficients.size() + degree, 0);

	normalize();
	return *this;
}

GenericGFPoly&
GenericGFPoly::divide(const GenericGFPoly& other, GenericGFPoly& quotient)
{
	assert(_field == other._field); // "GenericGFPolys do not have same GenericGF field"

	if (other.isZero())
		throw std::invalid_argument("Divide by 0");

	quotient.setField(*_field);
	quotient.setMonomial(0);
	auto& remainder = *this;

	const int inverseDenominatorLeadingTerm = _field->inverse(other.coefficient(other.degree()));

	ZX_THREAD_LOCAL GenericGFPoly temp;
	temp.setField(*_field);

	while (remainder.degree() >= other.degree() && !remainder.isZero()) {
		int degreeDifference = remainder.degree() - other.degree();
		int scale = _field->multiply(remainder.coefficient(remainder.degree()), inverseDenominatorLeadingTerm);
		temp.setMonomial(scale, degreeDifference);
		quotient.addOrSubtract(temp);
		temp = other;
		temp.multiplyByMonomial(degreeDifference, scale);
		remainder.addOrSubtract(temp);
	}

	return *this;
}

void GenericGFPoly::normalize()
{
	auto firstNonZero = FindIf(_coefficients, [](int c){ return c != 0; });
	// Leading term must be non-zero for anything except the constant polynomial "0"
	if (firstNonZero != _coefficients.begin()) {
		if (firstNonZero == _coefficients.end()) {
			_coefficients.resize(1, 0);
		} else {
			std::copy(firstNonZero, _coefficients.end(), _coefficients.begin());
			_coefficients.resize(_coefficients.end() - firstNonZero);
		}
	}
}

} // namespace ZXing
