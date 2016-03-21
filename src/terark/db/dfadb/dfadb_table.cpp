#include "dfadb_table.hpp"
#include "dfadb_segment.hpp"
#include <terark/fsa/create_regex_dfa.hpp>
#include <terark/io/FileStream.hpp>
#include <terark/io/DataIO.hpp>
#include <terark/num_to_str.hpp>
#include <terark/util/mmap.hpp>
#include <terark/db/mock_db_engine.hpp>
#include <terark/db/wiredtiger/wt_db_segment.hpp>
#include <terark/db/dfadb/nlt_index.hpp>
#include <boost/filesystem.hpp>

namespace terark { namespace db { namespace dfadb {


DfaDbContext::DfaDbContext(const CompositeTable* tab) : DbContext(tab) {
}
DfaDbContext::~DfaDbContext() {
}

DbContext* DfaDbTable::createDbContext() const {
	return new DfaDbContext(this);
}

ReadonlySegment*
DfaDbTable::createReadonlySegment(PathRef dir) const {
	std::unique_ptr<DfaDbReadonlySegment> seg(new DfaDbReadonlySegment());
	return seg.release();
}

WritableSegment*
DfaDbTable::createWritableSegment(PathRef dir) const {
	const char* dfaWritableSeg = getenv("TerarkDB_DfaWritableSegment");
	if (dfaWritableSeg && strcasecmp(dfaWritableSeg, "mock") == 0) {
		std::unique_ptr<WritableSegment> seg(new MockWritableSegment(dir));
		seg->m_schema = this->m_schema;
		return seg.release();
	}
	else {
		using terark::db::wt::WtWritableSegment;
		std::unique_ptr<WtWritableSegment> seg(new WtWritableSegment());
		seg->m_schema = this->m_schema;
		seg->load(dir);
		return seg.release();
	}
}

WritableSegment*
DfaDbTable::openWritableSegment(PathRef dir) const {
	auto isDelPath = dir / "isDel";
	if (boost::filesystem::exists(isDelPath)) {
		const char* dfaWritableSeg = getenv("TerarkDB_DfaWritableSegment");
		if (dfaWritableSeg && strcasecmp(dfaWritableSeg, "mock") == 0) {
			std::unique_ptr<WritableSegment> seg(new MockWritableSegment(dir));
			seg->m_schema = this->m_schema;
			seg->load(dir);
			return seg.release();
		}
		else {
			using terark::db::wt::WtWritableSegment;
			std::unique_ptr<WtWritableSegment> seg(new WtWritableSegment());
			seg->m_schema = this->m_schema;
			seg->load(dir);
			return seg.release();
		}
	}
	else {
		return myCreateWritableSegment(dir);
	}
}

bool
DfaDbTable::indexMatchRegex(size_t indexId, BaseDFA* regexDFA,
							valvec<llong>* recIdvec, DbContext* ctx)
const {
	if (indexId >= m_schema->getIndexNum()) {
		THROW_STD(invalid_argument
			, "invalid indexId=%zd is not less than indexNum=%zd"
			, indexId, m_schema->getIndexNum());
	}
	const Schema& schema = m_schema->getIndexSchema(indexId);
	if (schema.columnNum() > 1) {
		THROW_STD(invalid_argument
			, "can not MatchRegex on composite indexId=%zd indexName=%s"
			, indexId, schema.m_name.c_str());
	}
	auto& colmeta = schema.getColumnMeta(0);
	if (!colmeta.isString()) {
		THROW_STD(invalid_argument
			, "can not MatchRegex on non-string indexId=%zd indexName=%s"
			, indexId, schema.m_name.c_str());
	}
	recIdvec->erase_all();
	MyRwLock lock(this->m_rwMutex, false);
	for (size_t i = 0; i < m_segments.size(); ++i) {
		auto seg = m_segments[i].get();
		if (seg->getWritableStore()) {
			fprintf(stderr
				, "WARN: segment: %s is a writable segment, can not MatchRegex\n"
				, getSegPath("wr", i).string().c_str());
			continue;
		}
		auto index = dynamic_cast<const NestLoudsTrieIndex*>(&*seg->m_indices[indexId]);
		if (!index) {
			THROW_STD(logic_error, "MatchRegex must be run on NestLoudsTrieIndex\n");
		}
		size_t oldsize = recIdvec->size();
		if (index->matchRegexAppend(regexDFA, recIdvec, ctx)) {
			llong baseId = m_rowNumVec[i];
			size_t i = oldsize;
			for(size_t j = oldsize; j < recIdvec->size(); ++j) {
				size_t subPhysicId = (*recIdvec)[j];
				size_t subLogicId = seg->getLogicId(subPhysicId);
				if (!seg->m_isDel[subLogicId])
					(*recIdvec)[i++] = baseId + subLogicId;
			}
			recIdvec->risk_set_size(i);
		}
		else { // failed because exceeded memory limit
			// should fallback to use linear scan?
			fprintf(stderr
				, "RegexMatch exceeded memory limit(%zd bytes) on index '%s' of segment: '%s'\n"
				, ctx->regexMatchMemLimit
				, schema.m_name.c_str(), seg->m_segDir.string().c_str());
		}
	}
	return true;
}

bool
DfaDbTable::indexMatchRegex(size_t indexId,
							fstring regexStr, fstring regexOptions,
							valvec<llong>* recIdvec, DbContext* ctx)
const {
	std::unique_ptr<BaseDFA> regexDFA(create_regex_dfa(regexStr, regexOptions));
	return indexMatchRegex(indexId, regexDFA.get(), recIdvec, ctx);
}


TERARK_DB_REGISTER_TABLE_CLASS(DfaDbTable);

}}} // namespace terark::db::dfadb
