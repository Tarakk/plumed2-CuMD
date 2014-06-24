/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed-code.org for more information.

   This file is part of plumed, version 2.0.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#include "ReferenceArguments.h"
#include "ReferenceAtoms.h"
#include "tools/OFile.h"
#include "core/Value.h"

namespace PLMD {

ReferenceArguments::ReferenceArguments( const ReferenceConfigurationOptions& ro ):
ReferenceConfiguration(ro),
hasmetric(false),
hasweights(false)
{
}

void ReferenceArguments::readArgumentsFromPDB( const PDB& pdb ){
  parseVector( "ARG", arg_names );

  reference_args.resize( arg_names.size() );
  for(unsigned i=0;i<arg_names.size();++i) parse( arg_names[i], reference_args[i] );

  if( hasweights ){
      plumed_massert( !hasmetric, "should not have weights if we are using metric");
      weights.resize( arg_names.size() );
      for(unsigned i=0;i<reference_args.size();++i){
          parse( "sigma_" + arg_names[i], weights[i] ); 
      }
  } else if( hasmetric ){
      plumed_massert( !hasweights, "should not have weights if we are using metric");
      double thissig; metric.resize( arg_names.size(), arg_names.size() );
      for(unsigned i=0;i<reference_args.size();++i){
          for(unsigned j=i;j<reference_args.size();++j){
              parse( "sigma_" + arg_names[i] + "_" + arg_names[j], thissig );
              metric(i,j)=metric(j,i)=thissig;
          }
      }
  } else {
      weights.resize( arg_names.size() );
      for(unsigned i=0;i<weights.size();++i) weights[i]=1.0; 
  }
}

void ReferenceArguments::setArgumentNames( const std::vector<std::string>& arg_vals ){
  reference_args.resize( arg_vals.size() ); 
  arg_names.resize( arg_vals.size() ); 
  der_index.resize( arg_vals.size() );
  for(unsigned i=0;i<arg_vals.size();++i){
     arg_names[i]=arg_vals[i]; der_index[i]=i; 
  }
  if( hasmetric ) metric.resize( arg_vals.size(), arg_vals.size() );
  else weights.resize( arg_vals.size() );
}

void ReferenceArguments::setReferenceArguments( const std::vector<double>& arg_vals, const std::vector<double>& sigma ){
  plumed_dbg_assert( reference_args.size()==arg_vals.size() );
  for(unsigned i=0;i<arg_vals.size();++i) reference_args[i]=arg_vals[i];
  
  if( hasmetric ){
     unsigned k=0;
     for(unsigned i=0;i<reference_args.size();++i){ 
          for(unsigned j=i;j<reference_args.size();++j){
              metric(i,j)=metric(j,i)=sigma[k]; k++;
          }
     }
     plumed_assert( k==sigma.size() ); 
  } else {
     plumed_assert( reference_args.size()==sigma.size() );
     for(unsigned i=0;i<reference_args.size();++i) weights[i]=sigma[i];
  } 
}

void ReferenceArguments::getArgumentRequests( std::vector<std::string>& argout, bool disable_checks ){
  der_index.resize( arg_names.size() );

  if( argout.size()==0 ){
      for(unsigned i=0;i<arg_names.size();++i){
         argout.push_back( arg_names[i] );
         der_index[i]=i;
      }
  } else {
      if(!disable_checks){
         if( arg_names.size()!=argout.size() ) error("mismatched numbers of arguments in pdb frames");
      }
      bool found;
      for(unsigned i=0;i<arg_names.size();++i){
         found=false;
         if(!disable_checks){
            if( argout[i]!=arg_names[i] ) error("found mismatched arguments in pdb frames");
            der_index[i]=i;
         } else {
            for(unsigned j=0;j<arg_names.size();++j){
              if( argout[j]==arg_names[i] ){ found=true; der_index[i]=j; break; }
            }
            if( !found ){
              der_index[i]=argout.size(); argout.push_back( arg_names[i] );
            }
         }
      }
  }
}

void ReferenceArguments::printArguments( OFile& ofile, const std::string& fmt ) const {
  ofile.printf("REMARK ARG=%s", arg_names[0].c_str() );
  for(unsigned i=1;i<arg_names.size();++i) ofile.printf(",%s", arg_names[i].c_str() );
  ofile.printf("\n");
  ofile.printf("REMARK ");
  std::string descr2;
  if(fmt.find("-")!=std::string::npos){
     descr2="%s=" + fmt + " ";
  } else {
     // This ensures numbers are left justified (i.e. next to the equals sign
     std::size_t psign=fmt.find("%");
     plumed_assert( psign!=std::string::npos );
     descr2="%s=%-" + fmt.substr(psign+1) + " ";
  }
  for(unsigned i=0;i<arg_names.size();++i) ofile.printf( descr2.c_str(),arg_names[i].c_str(), reference_args[i] );
  ofile.printf("\n");

  // Missing print out of metrics
}

const std::vector<double>& ReferenceArguments::getReferenceMetric(){
  if( hasmetric ){
     unsigned ntot=(reference_args.size() / 2 )*(reference_args.size()+1);
     if( trig_metric.size()!=ntot ) trig_metric.resize( ntot );
     unsigned k=0;
     for(unsigned i=0;i<reference_args.size();++i){
         for(unsigned j=i;j<reference_args.size();++j){
             plumed_dbg_assert( fabs( metric(i,j)-metric(j,i) ) < epsilon );
             trig_metric[k]=metric(i,j); k++;
         }
     }
  } else {
    if( trig_metric.size()!=reference_args.size() ) trig_metric.resize( reference_args.size() );
    for(unsigned i=0;i<reference_args.size();++i) trig_metric[i]=weights[i];
  }
  return trig_metric;
}

double ReferenceArguments::calculateArgumentDistance( const std::vector<Value*> vals, const std::vector<double>& arg, const bool& squared ){
  double r=0;
  if( hasmetric ){
      double dp_i, dp_j;
      for(unsigned i=0;i<reference_args.size();++i){
          unsigned ik=der_index[i]; arg_ders[ ik ]=0;
          dp_i=vals[ik]->difference( reference_args[i], arg[ik] );
          for(unsigned j=0;j<reference_args.size();++j){
             unsigned jk=der_index[j];
             if(i==j) dp_j=dp_i;
             else dp_j=vals[jk]->difference( reference_args[j], arg[jk] );

             arg_ders[ ik ]+=metric(i,j)*dp_j;
             r+=dp_i*dp_j*metric(i,j);
          }
      }
  } else {
      double dp_i;
      for(unsigned i=0;i<reference_args.size();++i){
          unsigned ik=der_index[i];
          dp_i=vals[ik]->difference( reference_args[i], arg[ik] );
          r+=weights[i]*dp_i*dp_i; arg_ders[ik]=2.0*weights[i]*dp_i;
      }
  }
  if(!squared){ 
    r=sqrt(r); double ir=1.0/(2.0*r); 
    for(unsigned i=0;i<arg_ders.size();++i) arg_ders[i]*=ir; 
  }
  return r;
}
}