// =============================================================================
//  REL -- Shared Derive callback implementations
// =============================================================================

#include "operation/operation_helpers.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace rel
{
    namespace operation
    {

        using namespace xdataset;

        // =========================================================================
        //  DeriveShapeBroadcast
        // =========================================================================

        DataShape DeriveShapeBroadcast(const std::vector<DataShape>& operand_shapes)
        {
            DataKind res_kind = DataKind::kScalar;
            for (size_t i = 0; i < operand_shapes.size(); ++i)
            {
                DataKind k = operand_shapes[i].kind();
                if (k == DataKind::kMatrix)
                    res_kind = DataKind::kMatrix;
                else if (k == DataKind::kVector && res_kind != DataKind::kMatrix)
                    res_kind = DataKind::kVector;
            }

            if (res_kind == DataKind::kScalar)
                return DataShape::Scalar();

            if (res_kind == DataKind::kVector)
            {
                Index w = 1;
                for (size_t i = 0; i < operand_shapes.size(); ++i)
                {
                    if (operand_shapes[i].kind() == DataKind::kScalar)
                        continue;
                    Index sw = operand_shapes[i][0];
                    if (sw == 1)
                        continue;
                    if (w == 1)
                    {
                        w = sw;
                        continue;
                    }
                    if (sw != w)
                        throw std::invalid_argument("vector width mismatch (" + std::to_string(w) +
                                                    " vs " + std::to_string(sw) + ")");
                }
                return DataShape::Vector(w);
            }

            Index r = 1, c = 1;
            for (size_t i = 0; i < operand_shapes.size(); ++i)
            {
                Index op_r = 1, op_c = 1;
                DataKind k = operand_shapes[i].kind();
                const auto& s = operand_shapes[i];

                if (k == DataKind::kScalar)
                {
                    op_r = 1;
                    op_c = 1;
                }
                else if (k == DataKind::kVector)
                {
                    op_r = 1;
                    op_c = s[0];
                }
                else /* kMatrix */
                {
                    op_r = s[0];
                    op_c = s[1];
                }

                if (op_r == 1)
                { /* broadcast */
                }
                else if (r == 1)
                {
                    r = op_r;
                }
                else if (op_r != r)
                    throw std::invalid_argument("row dim mismatch (" + std::to_string(r) + " vs " +
                                                std::to_string(op_r) + ")");

                if (op_c == 1)
                { /* broadcast */
                }
                else if (c == 1)
                {
                    c = op_c;
                }
                else if (op_c != c)
                    throw std::invalid_argument("col dim mismatch (" + std::to_string(c) + " vs " +
                                                std::to_string(op_c) + ")");
            }
            return DataShape::Matrix(r, c);
        }

        // =========================================================================
        //  DeriveRowsBroadcast
        // =========================================================================

        Index DeriveRowsBroadcast(const std::vector<Index>& rows)
        {
            return RowBroadcastPlan::Compute(rows).result_size;
        }

        // =========================================================================
        //  DeriveDtypePromote  (conditional: Int only→Int, Real→Real, Complex→Complex)
        // =========================================================================

        DataType DeriveDtypePromote(const std::vector<DataType>& dtypes)
        {
            DataType res = DataType::kInteger;
            for (size_t i = 0; i < dtypes.size(); ++i)
            {
                DataType dt = (dtypes[i] == DataType::kBoolean) ? DataType::kInteger : dtypes[i];
                if (dt == DataType::kInteger)
                    continue;
                if (dt == DataType::kReal)
                    { if (res != DataType::kComplex) res = DataType::kReal; continue; }
                if (dt == DataType::kComplex)
                    { res = DataType::kComplex; continue; }
                throw std::runtime_error(std::string("unsupported type: ")
                    + DataTypeToString(dt) + ", expected Real, Integer, or Complex");
            }
            return res;
        }

        // =========================================================================
        //  DeriveDtypePromoteReal  (like Promote but Int �?Real always)
        // =========================================================================

        DataType DeriveDtypePromoteReal(const std::vector<DataType>& dtypes)
        {
            DataType res = DataType::kReal;
            for (size_t i = 0; i < dtypes.size(); ++i)
            {
                DataType dt = (dtypes[i] == DataType::kBoolean) ? DataType::kInteger : dtypes[i];
                if (dt == DataType::kInteger)
                    continue;
                if (dt == DataType::kReal)
                    { if (res != DataType::kComplex) res = DataType::kReal; continue; }
                if (dt == DataType::kComplex)
                    { res = DataType::kComplex; continue; }
                throw std::runtime_error(std::string("unsupported type: ")
                    + DataTypeToString(dt) + ", expected Real, Integer, or Complex");
            }
            return res;
        }

        // =========================================================================
        //  DeriveDtypePromoteNoComplex  (like Promote but reject Complex)
        // =========================================================================

        DataType DeriveDtypePromoteNoComplex(const std::vector<DataType>& dtypes)
        {
            DataType res = DataType::kInteger;
            for (size_t i = 0; i < dtypes.size(); ++i)
            {
                DataType dt = (dtypes[i] == DataType::kBoolean) ? DataType::kInteger : dtypes[i];
                if (dt == DataType::kInteger)
                    continue;
                if (dt == DataType::kReal && res != DataType::kComplex)
                    { res = DataType::kReal; continue; }
                throw std::runtime_error(std::string("unsupported type: ")
                    + DataTypeToString(dt) + ", expected Real or Integer");
            }
            return res;
        }

        // =========================================================================
        //  DeriveDtypePromoteWithString  (like Promote but all-String �?String; mix �?error)
        // =========================================================================

        DataType DeriveDtypePromoteWithString(const std::vector<DataType>& dtypes)
        {
            bool all_string = true;
            for (size_t i = 0; i < dtypes.size(); ++i)
            {
                DataType dt = (dtypes[i] == DataType::kBoolean) ? DataType::kInteger : dtypes[i];
                if (dt != DataType::kString) { all_string = false; break; }
            }
            if (all_string)
                return DataType::kString;

            for (size_t i = 0; i < dtypes.size(); ++i)
            {
                DataType dt = (dtypes[i] == DataType::kBoolean) ? DataType::kInteger : dtypes[i];
                if (dt == DataType::kString)
                    throw std::invalid_argument("cannot mix string with numeric types");
            }
            return DeriveDtypePromote(dtypes);
        }

        // =========================================================================
        //  DeriveDtypeRequireInt  (only Bool/Int �?Int; reject anything else)
        // =========================================================================

        DataType DeriveDtypeRequireInt(const std::vector<DataType>& dtypes)
        {
            for (size_t i = 0; i < dtypes.size(); ++i)
            {
                DataType dt = dtypes[i];
                if (dt == DataType::kBoolean || dt == DataType::kInteger)
                    continue;
                throw std::runtime_error(std::string("int operand required, got type ")
                    + DataTypeToString(dt));
            }
            return DataType::kInteger;
        }

        // =========================================================================
        //  DeriveDtypeForceReal  (all numeric �?Real, with validation)
        // =========================================================================

        DataType DeriveDtypeForceReal(const std::vector<DataType>& dtypes)
        {
            for (size_t i = 0; i < dtypes.size(); ++i)
            {
                DataType dt = (dtypes[i] == DataType::kBoolean) ? DataType::kInteger : dtypes[i];
                if (dt == DataType::kInteger || dt == DataType::kReal || dt == DataType::kComplex)
                    continue;
                throw std::runtime_error(std::string("unsupported type: ")
                    + DataTypeToString(dt) + ", expected Real, Integer, or Complex");
            }
            return DataType::kReal;
        }

        // =========================================================================
        //  DeriveDtypeForceRealNoComplex  (Int/Real �?Real, reject Complex)
        // =========================================================================

        DataType DeriveDtypeForceRealNoComplex(const std::vector<DataType>& dtypes)
        {
            for (size_t i = 0; i < dtypes.size(); ++i)
            {
                DataType dt = (dtypes[i] == DataType::kBoolean) ? DataType::kInteger : dtypes[i];
                if (dt == DataType::kInteger || dt == DataType::kReal)
                    continue;
                throw std::runtime_error(std::string("unsupported type: ")
                    + DataTypeToString(dt) + ", expected Real or Integer");
            }
            return DataType::kReal;
        }

        // =========================================================================
        //  DeriveDtypeForceIntNoComplex  (Int/Real → Int, reject Complex)
        // =========================================================================

        DataType DeriveDtypeForceIntNoComplex(const std::vector<DataType>& dtypes)
        {
            for (size_t i = 0; i < dtypes.size(); ++i)
            {
                DataType dt = (dtypes[i] == DataType::kBoolean) ? DataType::kInteger : dtypes[i];
                if (dt == DataType::kInteger || dt == DataType::kReal)
                    continue;
                throw std::runtime_error(std::string("unsupported type: ")
                    + DataTypeToString(dt) + ", expected Real or Integer");
            }
            return DataType::kInteger;
        }

        // =========================================================================
        //  DeriveDtypeComplexToReal  (Int→Int, Real→Real, Complex→Real)
        // =========================================================================

        DataType DeriveDtypeComplexToReal(const std::vector<DataType>& dtypes)
        {
            DataType dt = (dtypes[0] == DataType::kBoolean) ? DataType::kInteger : dtypes[0];
            if (dt == DataType::kInteger || dt == DataType::kReal)
                return dt;
            if (dt == DataType::kComplex)
                return DataType::kReal;
            throw std::runtime_error(std::string("unsupported type: ")
                + DataTypeToString(dt) + ", expected Real, Integer, or Complex");
        }

        // =========================================================================
        //  DeriveUnitPromoteDimension
        // =========================================================================

        Unit DeriveUnitPromoteDimension(const std::vector<Unit>& units)
        {
            if (units.empty())
                return Unit();
            Unit res = units[0];
            for (size_t i = 1; i < units.size(); ++i)
            {
                if (res.same_dimension(units[i]))
                    continue;
                if (!res.has_dimension())
                {
                    res = units[i];
                    continue;
                }
                if (!units[i].has_dimension())
                    continue;
                throw std::invalid_argument("unit dimension mismatch");
            }
            return res;
        }

        // =========================================================================
        //  DeriveUnitMod  (SameDim check, result always left unit)
        // =========================================================================

        Unit DeriveUnitMod(const std::vector<Unit>& units)
        {
            DeriveUnitPromoteDimension(units);  // throws on mismatch
            return units[0];
        }

        // =========================================================================
        //  DeriveUnitDimlessRight  (left unit passthrough, right must be dimless)
        // =========================================================================

        Unit DeriveUnitDimlessRight(const std::vector<Unit>& units)
        {
            if (!units[1].has_dimension())
                return units[0];
            throw std::invalid_argument("right operand must be dimensionless");
        }

        Unit DeriveUnitMul(const std::vector<Unit>& units)
        {
            Unit res = units[0];
            for (size_t i = 1; i < units.size(); ++i)
                res = res * units[i];
            return res;
        }

        Unit DeriveUnitDiv(const std::vector<Unit>& units)
        {
            Unit res = units[0];
            for (size_t i = 1; i < units.size(); ++i)
                res = res / units[i];
            return res;
        }

        Unit DeriveUnitDimless(const std::vector<Unit>& /*units*/)
        {
            return Unit();
        }

    } // namespace operation
} // namespace rel
