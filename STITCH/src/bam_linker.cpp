#include <Rcpp.h>
#include <iostream>

// the following "workaround" is needed as I cannot include both
// seqLib and Rcpp in the same .cpp for reasons I do not understand

std::tuple<std::vector<std::string>, std::vector<std::string>, std::vector<int>, std::vector<int>, std::vector<std::string>, std::vector<int>, std::vector<std::string>, std::vector<std::string>> get_reads_from_seqLib(std::string region, std::string file_name, std::string ref_fa);

std::tuple<std::vector<int>, std::vector<int>, std::vector<int>, std::vector<int>, std::vector<std::string>, std::vector<std::string>> get_sampleReadsRaw_using_SeqLib(const bool useSoftClippedBases, const int bqFilter, const int iSizeUpperLimit, std::vector<std::string> ref, std::vector<std::string> alt, const int T, std::vector<int> L, std::string region, std::string file_name, std::string reference);

std::tuple<std::vector<int>, std::vector<std::string>> split_cigar(std::string in_string);

std::tuple<std::vector<int>, std::vector<std::string>, std::string, std::string, int> deal_with_soft_clipped_bases(std::vector<int> cigarLengthVec, std::vector<std::string> cigarTypeVec, std::string seq, std::string qual, int pos, bool useSoftClippedBases);



//' @export
// [[Rcpp::export]]
Rcpp::List cpp_cigar_split_many(std::vector <std::string> strings) {
  // the only purpose of this is to make an R friendly wrapper to split_cigar
  // this is only using during testing
  int num_strings = strings.size();
  Rcpp::List out(num_strings);  
  std::vector<int> cigarLengthVec;
  std::vector<std::string> cigarTypeVec;
  for(int i=0; i < num_strings; i++ ) {
      std::tie(cigarLengthVec, cigarTypeVec) = split_cigar(strings[i]);
      out[i] = Rcpp::List::create(cigarLengthVec, cigarTypeVec);
  }
  return out;
}





//' @export
// [[Rcpp::export]]
Rcpp::List cpp_deal_with_soft_clipped_bases(Rcpp::List splitCigarRead, bool useSoftClippedBases, int posRead, std::string seqRead, std::string qualRead) {

  // only work on one at a time
  // so probably slow if used, but not really intended to be used at scale
  // only for tests

  std::vector<int> cigarLengthVec_out;
  std::vector<std::string> cigarTypeVec_out;
  int posRead_out;
  std::string seqRead_out;
  std::string qualRead_out;  
    
  std::vector<int> cigarLengthVec = Rcpp::as<std::vector<int>>(splitCigarRead[0]);
  std::vector<std::string> cigarTypeVec = Rcpp::as<std::vector<std::string>>(splitCigarRead[1]);
  std::tie(cigarLengthVec_out, cigarTypeVec_out, seqRead_out, qualRead_out, posRead_out) = deal_with_soft_clipped_bases(cigarLengthVec, cigarTypeVec, seqRead, qualRead, posRead, useSoftClippedBases);
  Rcpp::List splitCigarRead_out = Rcpp::List::create(cigarLengthVec_out, cigarTypeVec_out);
  
  return Rcpp::List::create(
      Rcpp::Named("splitCigarRead") = splitCigarRead_out,
      Rcpp::Named("posRead") = posRead_out,
      Rcpp::Named("seqRead") = seqRead_out,
      Rcpp::Named("qualRead") = qualRead_out      
  );
}






//' @export
// [[Rcpp::export]]
Rcpp::List get_sample_data_from_SeqLib(std::string region, std::string file_name, std::string reference = "") {
    std::vector<std::string> qname;
    std::vector<std::string> strand;
    std::vector<int> pos;
    std::vector<int> mapq;
    std::vector<std::string> cigar;
    std::vector<int> isize;
    std::vector<std::string> seq;
    std::vector<std::string> qual;
    std::tie(qname, strand, pos, mapq, cigar, isize, seq, qual) = get_reads_from_seqLib(region, file_name, reference) ;
    return Rcpp::List::create(
        Rcpp::Named("qname") = qname,
	Rcpp::Named("strand") = strand,
	Rcpp::Named("pos") = pos,
	Rcpp::Named("mapq") = mapq,
	Rcpp::Named("cigar") = cigar,
	Rcpp::Named("isize") = isize,
	Rcpp::Named("seq") = seq,
	Rcpp::Named("qual") = qual
    );
}



//' @export
// [[Rcpp::export]]
Rcpp::List get_sampleReadsRaw_from_SeqLib(const bool useSoftClippedBases, const int bqFilter, const int iSizeUpperLimit, const std::vector<std::string> ref, const std::vector<std::string> alt, const int T, const std::vector<int> L, std::string region, std::string file_name, std::string reference = "") {
  
  std::vector<int> out_num_SNPs; // 0-based
  std::vector<int> out_BQs; // pred-scaled base qualities, scaled with <0 -> ref, >0 -> alt
  std::vector<int> out_SNP_pos; // 0-based position of SNPs
  std::vector<int> out_iRead; // 0-based read SNPs came from
  std::vector<std::string> qname; // read name for reads with SNPs in them
  std::vector<std::string> strand; // strand for reads with SNPs in them


  std::tie(out_num_SNPs, out_BQs, out_SNP_pos, out_iRead, qname, strand) = get_sampleReadsRaw_using_SeqLib(useSoftClippedBases, bqFilter, iSizeUpperLimit, ref, alt, T, L, region, file_name, reference);

  //
  // convert format to sampleReadsRaw
  //
  Rcpp::List sampleReadsRaw;
  int flush_read;
  int nSNPInRead;
  int iRead_out = -1;
  int n = out_num_SNPs.size();
  nSNPInRead = 0;
  //std::cout << "n=" << n << std::endl;
  std::vector<int> qR;
  std::vector<int> pR;
  for(int iRead = 0; iRead < n; iRead++) {
      //std::cout << "iRead=" << iRead << std::endl;    
      // either flush the read, or keep going
      if (iRead == (n - 1)) {
	  flush_read = 1;
      } else if (out_iRead[iRead + 1] != out_iRead[iRead]) {
          flush_read = 1;
      } else {
	flush_read = 0;
      }
      //std::cout << "out_num_SNPs[iRead]=" << out_num_SNPs[iRead] << std::endl;
      //std::cout << "out_iRead[iRead]=" << out_iRead[iRead] << std::endl;
      //std::cout << "out_BQs[iRead]=" << out_BQs[iRead] << std::endl;
      //std::cout << "out_SNP_pos[iRead]=" << out_SNP_pos[iRead] << std::endl;
      //std::cout << "flush_read=" << flush_read << std::endl; 
      // 0 - keep building read
      nSNPInRead = out_num_SNPs[iRead];
      iRead_out = out_iRead[iRead];
      qR.push_back(out_BQs[iRead]);
      pR.push_back(out_SNP_pos[iRead]);
      if (flush_read == 1) {
	sampleReadsRaw.push_back(
            Rcpp::List::create(nSNPInRead, 0, qR, pR, iRead_out)
        );
	qR.clear();
	pR.clear();
      }
  }

    return Rcpp::List::create(
        Rcpp::Named("sampleReadsRaw") = sampleReadsRaw,
	Rcpp::Named("qname") = qname,
	Rcpp::Named("strand") = strand
    );

}
