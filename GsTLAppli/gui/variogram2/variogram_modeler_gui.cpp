/**********************************************************************
** Author: Nicolas Remy
** Copyright (C) 2002-2004 The Board of Trustees of the Leland Stanford Junior
**   University
** All rights reserved.
**
** This file is part of the "gui" module of the Geostatistical Earth
** Modeling Software (GEMS)
**
** This file may be distributed and/or modified under the terms of the 
** license defined by the Stanford Center for Reservoir Forecasting and 
** appearing in the file LICENSE.XFREE included in the packaging of this file.
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.gnu.org/copyleft/gpl.html for GPL licensing information.
**
** Contact the Stanford Center for Reservoir Forecasting, Stanford University
** if any conditions of this licensing are not clear to you.
**
**********************************************************************/

#include <GsTLAppli/gui/variogram2/variogram_modeler_gui.h>
#include <GsTLAppli/geostat/pset_variog_computer.h>
#include <GsTLAppli/geostat/grid_variog_computer.h>
#include <GsTLAppli/math/direction_3d.h>
#include <GsTLAppli/math/correlation_measure.h>
#include <GsTLAppli/utils/gstl_messages.h>
#include <GsTLAppli/utils/string_manipulation.h>
#include <GsTLAppli/appli/utilities.h>
#include <GsTLAppli/math/angle_convention.h>

#include <GsTL/geometry/geometry_algorithms.h>

#include <qtablewidget.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qscrollarea.h>
#include <qstringlist.h>
#include <QComboBox>

#include <algorithm>
#include <iterator>

using namespace String_Op;

Variogram_modeler_gui::
Variogram_modeler_gui( GsTL_project* project, QWidget* parent, const char* name )
  : QDialog( parent)
{
    setupUi(this);
    setWindowFlags(Qt::WindowMinMaxButtonsHint);
    
    setWindowTitle( "Variogram Modeler" );
    if (name)
      setObjectName(name);

    
    setSizeGripEnabled( true );

    project_=project;
    first_screen_scrollview_ = new QScrollArea( this);
    f_ = new First_scr( project_, first_screen_scrollview_->viewport() );
    first_screen_scrollview_->setWidget( f_ );

    //revisit - TL
    //first_screen_scrollview_->setResizePolicy( Q3ScrollView::AutoOneFit );
    
    //first_screen_scrollview_->layout()->setMargin( 1 );

    pset_params_screen_ = new QScrollArea( this);
    pset_params_=new Pset_params( pset_params_screen_->viewport() );
    pset_params_screen_->setWidget( pset_params_ );
    
    //revisit - TL
    //pset_params_screen_->setResizePolicy( Q3ScrollView::AutoOneFit );
    
    //pset_params_screen_->layout()->setMargin( 1 );

    rgrid_params_screen_ = new QScrollArea( this);
    rgrid_params_ = new RGrid_params( rgrid_params_screen_->viewport() );
    rgrid_params_screen_->setWidget( rgrid_params_ );
    
    //revisit - TL
    //rgrid_params_screen_->setResizePolicy( Q3ScrollView::AutoOneFit );
    //rgrid_params_screen_->layout()->setMargin( 1 );

    s_=new Variogram_modeling_screen;

    varg_ = 0;  
    grid_ = 0;
   
    // flag to indicate if parameters changed 
    changed_=true;
    variograms_computed_ = false;

    // remove the default widget
    _stack->removeWidget(_stack->widget(0));
    _stack->addWidget(first_screen_scrollview_);
    _stack->addWidget(pset_params_screen_);
    _stack->addWidget(rgrid_params_screen_);
    _stack->addWidget(s_);
    _stack->setCurrentIndex(0);

    _next->setFocusPolicy(Qt::NoFocus);
    _back->setFocusPolicy(Qt::NoFocus);
    _cancel->setFocusPolicy(Qt::NoFocus);
    
    QObject::connect( _next, SIGNAL(clicked()), this, SLOT(next()));
    QObject::connect( _back, SIGNAL(clicked()), this, SLOT(back()));
    QObject::connect( _cancel, SIGNAL(clicked()), this, SLOT(reject()));
    
    QObject::connect( pset_params_, SIGNAL(parameterChanged()),
		                  this, SLOT(parameterChanged()) );
    QObject::connect( rgrid_params_, SIGNAL(parameterChanged()),
		                  this,SLOT(parameterChanged()) );
    QObject::connect( f_, SIGNAL(parameterChanged()),
		                  this,SLOT(parameterChanged()) );

    _back->setEnabled(false);
    _next->setFocusPolicy( Qt::NoFocus );
    this->resize( 500, 500 ); 
}


Variogram_modeler_gui::~Variogram_modeler_gui()
{
    delete s_;
    delete varg_;
//    delete first_screen_scrollview_;
//    delete pset_params_screen_;
//    delete rgrid_params_screen_;
}


void Variogram_modeler_gui::parameterChanged() {
  changed_=true;
  disable_next_button_focus();
}

void Variogram_modeler_gui::disable_next_button_focus() {
  _next->setFocusPolicy( Qt::NoFocus );
}

void Variogram_modeler_gui::updateButtons()
{
  if (_stack->currentIndex() == 0) {
    _back->setEnabled(false);
  }
  else if (_stack->currentIndex() == _stack->count()-1) {
    _next->setEnabled(false);
  }
  else {
    _back->setEnabled(true);
    _next->setEnabled(true);
  }

}

void Variogram_modeler_gui::back() {
  if(_stack->currentWidget()==s_ && !f_->skip_variogram_computation() ) {
    if(dynamic_cast<Point_set *> (grid_)) {
	    _stack->setCurrentWidget( pset_params_screen_ );
	    changed_=false;
	  }
    else {
	    _stack->setCurrentWidget( rgrid_params_screen_ );
	    changed_=false;
	  }
  }
  else {
	  _stack->setCurrentWidget(first_screen_scrollview_);
	  changed_=false;
  }

  updateButtons();
  
  _next->setFocusPolicy( Qt::NoFocus );
}



void Variogram_modeler_gui::next() {

  if( _stack->currentWidget() == first_screen_scrollview_ ) {	
    if( ! f_->skip_variogram_computation() ) {
      if( changed_ ) 
        clear_variogram_modeling_screen();

      prompt_for_parameters();
    }
    else {
      if( !f_->load_experimental_variograms() ) return;
      
      if( !changed_ && variograms_computed_ == true ) {
        _stack->setCurrentWidget(s_);
        return;
      }

      int index = _stack->indexOf(s_);
      delete s_;
      s_=new Variogram_modeling_screen;
      _stack->removeWidget(s_);
      _stack->insertWidget(index,s_);

      delete varg_;
      varg_=new Variogram_function_adaptor<Covariance<GsTLPoint> >;
	    s_->run( f_->experimental_variograms(), varg_, varg_->covariance(),
               f_->directions(), f_->pairs(), 
               f_->parameters() );
      variograms_computed_ = true;
      _stack->setCurrentWidget(s_);
    }
  }
  else {
    // the user pressed Next on the parameter page
    // If new parameters were entered, we need to compute the requested variograms
    if( changed_ || variograms_computed_ == false ) {
      recompute_variograms();
    }  
    else  //no parameter was changed 
      _stack->setCurrentWidget(s_);
  }

  _next->setFocusPolicy( Qt::NoFocus );
  updateButtons();
}
    

void Variogram_modeler_gui::clear_variogram_modeling_screen() {
  if( !s_ ) return;

  int index = _stack->indexOf( s_ );
  delete s_;
  s_ = new Variogram_modeling_screen;
  _stack->removeWidget(s_);
  _stack->insertWidget(index,s_) ;
  
  variograms_computed_ = false;
}



void Variogram_modeler_gui::prompt_for_parameters() {
  grid_ = dynamic_cast<Geostat_grid*> (
   	  Root::instance()->interface(
         gridModels_manager + "/" + f_->grid_name()
       ).raw_ptr()
     );

  if( !grid_ ) {
    // Do some error handling
    return;
  }

  std::string tmp = f_->head_property() + " vs. " + f_->tail_property() ;
  this->setWindowTitle(  tmp.c_str());

  if ( dynamic_cast<Point_set *> (grid_) ) {
    _stack->setCurrentWidget( pset_params_screen_ );
    return;
  }
  if ( dynamic_cast< Strati_grid* >( grid_ ) ) {
    _stack->setCurrentWidget( rgrid_params_screen_ );
    return;
  }
}


void  
Variogram_modeler_gui::recompute_variograms() {
  QStringList parameters_list;
  std::vector<Discrete_function> df;
  std::vector<GsTLVector<double> >v;
  std::vector< std::vector<int> > pairs;

  //--------------
  // recompute the variograms
  bool parameters_ok = false;
  if(dynamic_cast<Point_set *> (grid_)) 
    parameters_ok = compute_df_pset( df, v, pairs, parameters_list );
  else 
 		parameters_ok = compute_df_rgrid( df, v, pairs, parameters_list );

  if( !parameters_ok ) return; 

  //--------------
  // set up the modeling page (remove the old one, create a new
  // one, remove the old variogram model, etc

  int index = _stack->indexOf(s_);
  _stack->removeWidget(s_);
  delete s_;
  s_=new Variogram_modeling_screen;
  _stack->insertWidget(index,s_);


  delete varg_;
  varg_=new Variogram_function_adaptor<Covariance<GsTLPoint> >;
	s_->run(df,varg_,varg_->covariance(),v, pairs, parameters_list );
  variograms_computed_ = true;

  _stack->setCurrentWidget(s_);
}



bool  
Variogram_modeler_gui::
compute_df_rgrid( std::vector<Discrete_function> &df,
			            std::vector<GsTLVector<double> > &v,
                  std::vector< std::vector<int> >& pairs,
                  QStringList& parameters_list ) 
{
  Discrete_function df_elem; 

  Grid_variog_computer variog_computer( (Strati_grid*) grid_, 
                                        grid_->property( f_->head_property() ),
                                        grid_->property( f_->tail_property() ) );

  std::vector< GsTLVector<double> > directions;
  std::vector<std::string> model_types;
  std::vector<std::pair<double, double> >  mod_param;
  rgrid_params_->all_parameters( directions, model_types, mod_param ); 
    
	variog_computer.standardize( rgrid_params_->scale_with_variance() );


  // check that the grid and the properties are still valid
  if( !grid_ ) {
    GsTLcerr << "The selected grid do not exist anymore.\n"
             << "Go back to the first screen and select a valid grid"
             << gstlIO::end;
   return false;
  }
  if( !grid_->property( f_->head_property() ) || 
      !grid_->property( f_->tail_property() )   ){
    GsTLcerr << "The selected properties do not exist anymore.\n"
             << "Go back to the first screen and select a valid properties"
             << gstlIO::end;
    return false;
  }

  //--------------------
  // compute variograms in every requested direction
    
  int total_steps =
    directions.size() * rgrid_params_->num_lags() * grid_->size();
  int frequency = std::max( total_steps / 20, 1 );
  SmartPtr<Progress_notifier> progress_notifier = 
    utils::create_notifier( "Computing variogram", 
                            total_steps, frequency );

  for( unsigned int i=0 ; i< directions.size() ; i++ ) {

    Correlation_measure* correlation_measure = 
      Correlation_measure_factory::instantiate( model_types[i] );
    
    if( !correlation_measure ) {
      GsTLcerr << "A correlation measure of type " << model_types[i] 
               << " could not be instantiated" << gstlIO::end;
      return false;
    }
    
    std::vector<double> correl_measure_params;
    correl_measure_params.push_back( mod_param[i].first );
    correl_measure_params.push_back( mod_param[i].second );
    correlation_measure->set_parameters( correl_measure_params );

    std::vector<int> pairs_count =
      variog_computer.compute_variogram_values( df_elem, 
                                                directions[i],
                                                rgrid_params_->num_lags(),
                                                correlation_measure,
                                                progress_notifier.raw_ptr() );
	
  	df.push_back(df_elem);
    pairs.push_back( pairs_count );
      
    directions[i] = directions[i] * ( 1 / euclidean_norm( directions[i] ) );
  	v.push_back( directions[i] );

    QString params( "(" );
    QString val;
    for( int k=0; k<3; k++ ) {
      val.setNum( (directions[i])[k] );
      params.append( val );
      if( k != 2 ) params.append( ", " );
    }
    params.append( ")" );
    parameters_list << params;

  }			    

  return true;
}




/// alpha is in degrees
double bandwidth_to_coneheight( double bandwidth, double alpha ) {
  if( fabs( alpha ) >= 90.0 ) return 0;

  alpha = degree_to_radian( alpha );
  return bandwidth / tan(alpha);
}



bool  
Variogram_modeler_gui::
compute_df_pset( std::vector<Discrete_function> &df,
          			 std::vector<GsTLVector<double> > &v,
                 std::vector< std::vector<int> >& pairs,
                 QStringList& parameters_list ) {

  std::vector<double> lags;
  std::vector<double> lag_tol;
    
  std::vector<double> angle1;
  std::vector<double> angle2;
  std::vector<double> angle_tol;
  std::vector<double> cone_ht;
  std::vector<std::string> model_type;
  std::vector<std::pair<double,double> >  mod_param;
    
		    
  const QTableWidget * angle_table = pset_params_->angle_table();
    
  double tol=pset_params_->lag_tol();
  double sep=pset_params_->lag_sep();
  int numlags=pset_params_->num_lags();  
    
  if( tol < 0 ) {
    GsTLcerr << "Lag tolerance must be positive" << gstlIO::end;
    return false;
  }
  if( sep < 0 ) {
    GsTLcerr << "Lag separation must be positive" << gstlIO::end;
    return false;
  }
  if( numlags < 0 ) {
    GsTLcerr << "The number of lags must be positive " << gstlIO::end;
    return false;
  }

  for( int i=0 ; i < numlags ; i++ ) {
    lags.push_back(sep+ i*sep);
    lag_tol.push_back(tol);			
  }
    

  //========================
  // Extract the variogram parameters

  for( int i2=0 ; i2 < angle_table->rowCount() ; i2++ ) {

    if(!angle_table->item(i2,0)->text().isEmpty()) { 
      QComboBox* combo_item = (QComboBox*) angle_table->indexWidget(angle_table->model()->index(i2,4,QModelIndex()));
      model_type.push_back( qstring2string(combo_item->currentText()) );	

      QString params( combo_item->currentText() + " - ");
      if( angle_table->item(i2,2)->text( ).toDouble() >= 90.0 ) {
        params.append( "Omni-directional" );
      }
      else {
        params.append( "azth=" + angle_table->item(i2,0)->text( ) );
        params.append( ", dip=" + angle_table->item(i2,1)->text() );
      }

      angle1.push_back((angle_table->item(i2,0)->text()).toDouble());
      angle2.push_back((angle_table->item(i2,1)->text()).toDouble());

      bool is_a_number;
      double angle_tolerance = angle_table->item(i2,2)->text().toDouble(&is_a_number);
      if( angle_tolerance < 0 || !is_a_number ) {
        GsTLcerr << "Enter a positive angle tolerance" << gstlIO::end;
        return false;
      }
      angle_tol.push_back( angle_tolerance );

      double bandwidth = (angle_table->item(i2,3)->text()).toDouble(&is_a_number);
      if( bandwidth < 0 && angle_tolerance < 90 || !is_a_number ) {
        GsTLcerr << "Enter a positive bandwith" << gstlIO::end;
        return false;
      }
      double cone_height = 
        bandwidth_to_coneheight( bandwidth, (angle_table->item(i2,2)->text()).toDouble() );
      cone_ht.push_back( cone_height );

      // get the indicator variogram parameters
      const double nan = -9e30;
      double thresh1=nan, thresh2=nan;
      if( combo_item->currentText().contains( "indic", Qt::CaseInsensitive ) ) {
        bool ok;
        thresh1 = angle_table->item(i2,5)->text().toDouble( &ok );
        if( ok )
          params.append( " - head cutoff=" +  angle_table->item(i2,5)->text() );
        else
          thresh1 = nan;

        thresh2 = angle_table->item(i2,6)->text().toDouble( &ok );
        if( ok )
          params.append( ", tail cutoff=" +  angle_table->item(i2,6)->text() );
        else
          thresh2 = nan;
      }
      mod_param.push_back( std::make_pair( thresh1, thresh2 ) );

      parameters_list << params;
    }
  }
    
     
  // check that the grid and the properties are still valid
  if( !grid_ ) {
    GsTLcerr << "The selected point-set do not exist anymore.\n"
             << "Go back to the first screen and select a valid point-set"
             << gstlIO::end;
   return false;
  }
  if( !grid_->property( f_->head_property() ) || 
      !grid_->property( f_->tail_property() )   ) {
    GsTLcerr << "The selected properties do not exist anymore.\n"
             << "Go back to the first screen and select a valid properties"
             << gstlIO::end;
    return false;
  }


 //==========================
 // compute variograms in every requested direction

  Pset_variog_computer variog_computer( (Point_set*) grid_, 
                                         grid_->property( f_->head_property() ),
                                         grid_->property( f_->tail_property() ) );
  variog_computer.standardize( pset_params_->standardize_sill() );

  //------------
  // set up the progress notifier
  int total_steps = angle1.size() * grid_->size() * (grid_->size()-1)/2;
  int frequency = std::max( total_steps / 20, 1 );
  SmartPtr<Progress_notifier> progress_notifier = 
    utils::create_notifier( "Computing variogram", 
                            total_steps, frequency );

  //------------
  // loop on all directions
  for( unsigned int k = 0 ; k < angle1.size() ; k++) {
  	Discrete_function df_elem; 
    df_elem.set_x_values(lags);
  	std::pair<double,double> angles;
    angles.first = degree_to_radian( angle1[k] );  
   	angles.second = degree_to_radian( angle2[k] ); 
    convert_to_math_standard_angles_rad( angles.first, angles.second );

    Direction_3d dir;
    dir.set_direction( angles.first, angles.second );
    dir.set_tolerance( degree_to_radian( angle_tol[k] ), cone_ht[k] );
    std::cout << "angle tol: " << angle_tol[k] << std::endl;

    Correlation_measure* correlation_measure = 
      Correlation_measure_factory::instantiate( model_type[k] );
    
    if( !correlation_measure ) {
      GsTLcerr << "A correlation measure of type " << model_type[k] 
               << " could not be instantiated" << gstlIO::end;
      return false;
    }
    
    std::vector<double> params;
    params.push_back( mod_param[k].first );
    params.push_back( mod_param[k].second );
    correlation_measure->set_parameters( params );

    std::vector<int> pairs_count =
      variog_computer.compute_variogram_values( df_elem,
                                                lag_tol, dir, 
                                                correlation_measure,
                                                progress_notifier.raw_ptr() );
    df.push_back(df_elem);
	  pairs.push_back( pairs_count );

   	GsTLVector<double> temp;
   	temp.x()=cos(angles.first)*cos(angles.second);
   	temp.y()=sin(angles.first)*cos(angles.second);
   	temp.z()=sin(angles.second);
   	v.push_back(temp);
  }

  return true;
}





bool Variogram_modeler_gui::Error()
{
    return false;
}

