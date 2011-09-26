#include <iomanip>

#include <DB/DataStreams/IProfilingBlockInputStream.h>


namespace DB
{


void BlockStreamProfileInfo::update(Block & block)
{
	++blocks;
	rows += block.rows();
	for (size_t i = 0; i < block.columns(); ++i)
		bytes += block.getByPosition(i).column->byteSize();

	if (column_names.empty())
		column_names = block.dumpNames();
}


void BlockStreamProfileInfo::print(std::ostream & ostr) const
{
	Poco::Timestamp::TimeDiff nested_elapsed = 0;
	UInt64 nested_rows 		= 0;
	UInt64 nested_blocks 	= 0;
	UInt64 nested_bytes 	= 0;
	
	if (!nested_infos.empty())
	{
		for (BlockStreamProfileInfos::const_iterator it = nested_infos.begin(); it != nested_infos.end(); ++it)
		{
			if ((*it)->work_stopwatch.elapsed() > nested_elapsed)
				nested_elapsed = (*it)->work_stopwatch.elapsed();
			nested_rows 	+= (*it)->rows;
			nested_blocks	+= (*it)->blocks;
			nested_bytes 	+= (*it)->bytes;
		}
	}
	
	ostr 	<< std::fixed << std::setprecision(2)
			<< "Columns: " << column_names << std::endl
			<< "Elapsed:        " << work_stopwatch.elapsed() / 1000000.0 << " sec. "
			<< "(" << work_stopwatch.elapsed() * 100.0 / total_stopwatch.elapsed() << "%), " << std::endl;

	if (!nested_infos.empty())
		ostr<< "Elapsed (self): " << (work_stopwatch.elapsed() - nested_elapsed) / 1000000.0 << " sec. "
			<< "(" << (work_stopwatch.elapsed() - nested_elapsed) * 100.0 / total_stopwatch.elapsed() << "%), " << std::endl
			<< "Rows (in):      " << nested_rows << ", per second: " << nested_rows * 1000000 / work_stopwatch.elapsed() << ", " << std::endl
			<< "Blocks (in):    " << nested_blocks << ", per second: " << nested_blocks * 1000000.0 / work_stopwatch.elapsed() << ", " << std::endl
			<< "                " << nested_bytes / 1000000.0 << " MB (memory), "
				<< nested_bytes / work_stopwatch.elapsed() << " MB/s (memory), " << std::endl;
		
	ostr 	<< "Rows (out):     " << rows << ", per second: " << rows * 1000000 / work_stopwatch.elapsed() << ", " << std::endl
			<< "Blocks (out):   " << blocks << ", per second: " << blocks * 1000000.0 / work_stopwatch.elapsed() << ", " << std::endl
			<< "                " << bytes / 1000000.0 << " MB (memory), " << bytes / work_stopwatch.elapsed() << " MB/s (memory), " << std::endl
			<< "Average block size (out): " << rows / blocks << "." << std::endl;
}

	
Block IProfilingBlockInputStream::read()
{
	if (!info.started)
	{
		info.total_stopwatch.start();

		for (BlockInputStreams::const_iterator it = children.begin(); it != children.end(); ++it)
		if (const IProfilingBlockInputStream * child = dynamic_cast<const IProfilingBlockInputStream *>(&**it))
			info.nested_infos.push_back(&child->info);
		
		info.started = true;
	}
	
	info.work_stopwatch.start();
	Block res = readImpl();
	info.work_stopwatch.stop();

	if (res)
		info.update(res);

/*	if (res)
	{
		std::cerr << std::endl;
		std::cerr << getName() << std::endl;
		getInfo().print(std::cerr);
	}*/

	return res;
}
	

const BlockStreamProfileInfo & IProfilingBlockInputStream::getInfo() const
{
	return info;
}


}
