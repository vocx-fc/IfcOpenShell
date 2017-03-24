#include "CgalKernel.h"

bool IfcGeom::CgalKernel::convert(const IfcSchema::IfcExtrudedAreaSolid *l, cgal_shape_t &shape) {
  const double height = l->Depth() * getValue(GV_LENGTH_UNIT);
  if (height < getValue(GV_PRECISION)) {
    Logger::Message(Logger::LOG_ERROR, "Non-positive extrusion height encountered for:", l->entity);
    return false;
  }
  
  // Outer
  cgal_face_t bottom_face;
  if ( !convert_face(l->SweptArea(),bottom_face) ) return false;
//  std::cout << "Face vertices: " << face.outer.size() << std::endl;
  
  cgal_placement_t trsf;
  bool has_position = true;
#ifdef USE_IFC4
  has_position = l->hasPosition();
#endif
  if (has_position) {
    IfcGeom::CgalKernel::convert(l->Position(), trsf);
  }
  
  cgal_direction_t dir;
  convert(l->ExtrudedDirection(),dir);
  //  std::cout << "Direction: " << dir << std::endl;
  
  std::list<cgal_face_t> face_list;
  face_list.push_back(bottom_face);
  
  for (std::vector<Kernel::Point_3>::const_iterator current_vertex = bottom_face.outer.begin();
       current_vertex != bottom_face.outer.end();
       ++current_vertex) {
    std::vector<Kernel::Point_3>::const_iterator next_vertex = current_vertex;
    ++next_vertex;
    if (next_vertex == bottom_face.outer.end()) {
      next_vertex = bottom_face.outer.begin();
    } cgal_face_t side_face;
    side_face.outer.push_back(*next_vertex);
    side_face.outer.push_back(*current_vertex);
    side_face.outer.push_back(*current_vertex+height*dir);
    side_face.outer.push_back(*next_vertex+height*dir);
    face_list.push_back(side_face);
  }
  
  cgal_face_t top_face;
  for (std::vector<Kernel::Point_3>::const_reverse_iterator vertex = bottom_face.outer.rbegin();
       vertex != bottom_face.outer.rend();
       ++vertex) {
    top_face.outer.push_back(*vertex+height*dir);
  } face_list.push_back(top_face);
  
  if (bottom_face.inner.empty()) {
    shape = create_polyhedron(face_list);
    for (auto &vertex: vertices(shape)) vertex->point() = vertex->point().transform(trsf);
    return true;
  }
  
  CGAL::Nef_polyhedron_3<Kernel> nef_shape = create_nef_polyhedron(face_list);
  
  // Inner
  // TODO: Would be faster to triangulate top/bottom face template rather than use Nef polyhedra for subtraction
  for (auto &inner: bottom_face.inner) {
//    std::cout << "Inner wire" << std::endl;
    face_list.clear();
    
    cgal_face_t hole_bottom_face;
    hole_bottom_face.outer = inner;
    face_list.push_back(hole_bottom_face);
    
    for (std::vector<Kernel::Point_3>::const_iterator current_vertex = inner.begin();
         current_vertex != inner.end();
         ++current_vertex) {
      std::vector<Kernel::Point_3>::const_iterator next_vertex = current_vertex;
      ++next_vertex;
      if (next_vertex == inner.end()) {
        next_vertex = inner.begin();
      } cgal_face_t hole_side_face;
      hole_side_face.outer.push_back(*next_vertex);
      hole_side_face.outer.push_back(*current_vertex);
      hole_side_face.outer.push_back(*current_vertex+height*dir);
      hole_side_face.outer.push_back(*next_vertex+height*dir);
      face_list.push_back(hole_side_face);
    }
    
    cgal_face_t hole_top_face;
    for (std::vector<Kernel::Point_3>::const_reverse_iterator vertex = inner.rbegin();
         vertex != inner.rend();
         ++vertex) {
      hole_top_face.outer.push_back(*vertex+height*dir);
    } face_list.push_back(hole_top_face);
    
    nef_shape -= create_nef_polyhedron(face_list);
  }
  
  nef_shape.transform(trsf);
  nef_shape.convert_to_polyhedron(shape);
  return true;
}

bool IfcGeom::CgalKernel::convert(const IfcSchema::IfcConnectedFaceSet* l, cgal_shape_t& shape) {
  IfcSchema::IfcFace::list::ptr faces = l->CfsFaces();
  
  std::list<cgal_face_t> face_list;
  for (IfcSchema::IfcFace::list::it it = faces->begin(); it != faces->end(); ++it) {
    bool success = false;
    cgal_face_t face;
    
    try {
      success = convert_face(*it, face);
    } catch (...) {}
    
    if (!success) {
      Logger::Message(Logger::LOG_WARNING, "Failed to convert face:", (*it)->entity);
      continue;
    }
    
    //    std::cout << "Face in ConnectedFaceSet: " << std::endl;
    //    for (auto &point: face.outer) {
    //      std::cout << "\tPoint(" << point << ")" << std::endl;
    //    }
    
    face_list.push_back(face);
  }
  
  shape = create_polyhedron(face_list);
  return true;
}

bool IfcGeom::CgalKernel::convert(const IfcSchema::IfcCsgSolid* l, cgal_shape_t& shape) {
  return convert_shape(l->TreeRootExpression(), shape);
}

bool IfcGeom::CgalKernel::convert(const IfcSchema::IfcBlock* l, cgal_shape_t& shape) {
  const double dx = l->XLength() * getValue(GV_LENGTH_UNIT);
  const double dy = l->YLength() * getValue(GV_LENGTH_UNIT);
  const double dz = l->ZLength() * getValue(GV_LENGTH_UNIT);
  
  std::list<cgal_face_t> face_list;
  
  // x = 0
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(Kernel::Point_3(0, 0, 0));
  face_list.back().outer.push_back(Kernel::Point_3(0, dy, 0));
  face_list.back().outer.push_back(Kernel::Point_3(0, dy, dz));
  face_list.back().outer.push_back(Kernel::Point_3(0, 0, dz));
  
  // x = dx
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(Kernel::Point_3(dx, 0, 0));
  face_list.back().outer.push_back(Kernel::Point_3(dx, 0, dz));
  face_list.back().outer.push_back(Kernel::Point_3(dx, dy, dz));
  face_list.back().outer.push_back(Kernel::Point_3(dx, dy, 0));
  
  // y = 0
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(Kernel::Point_3(0, 0, 0));
  face_list.back().outer.push_back(Kernel::Point_3(0, 0, dz));
  face_list.back().outer.push_back(Kernel::Point_3(dx, 0, dz));
  face_list.back().outer.push_back(Kernel::Point_3(dx, 0, 0));
  
  // y = dy
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(Kernel::Point_3(0, dy, 0));
  face_list.back().outer.push_back(Kernel::Point_3(dx, dy, 0));
  face_list.back().outer.push_back(Kernel::Point_3(dx, dy, dz));
  face_list.back().outer.push_back(Kernel::Point_3(0, dy, dz));
  
  // z = 0
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(Kernel::Point_3(0, 0, 0));
  face_list.back().outer.push_back(Kernel::Point_3(dx, 0, 0));
  face_list.back().outer.push_back(Kernel::Point_3(dx, dy, 0));
  face_list.back().outer.push_back(Kernel::Point_3(0, dy, 0));
  
  // z = dz
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(Kernel::Point_3(0, 0, dz));
  face_list.back().outer.push_back(Kernel::Point_3(0, dy, dz));
  face_list.back().outer.push_back(Kernel::Point_3(dx, dy, dz));
  face_list.back().outer.push_back(Kernel::Point_3(dx, 0, dz));
  
  cgal_placement_t trsf;
  IfcGeom::CgalKernel::convert(l->Position(),trsf);

  shape = create_polyhedron(face_list);
  for (auto &vertex: vertices(shape)) vertex->point() = vertex->point().transform(trsf);
  return true;
}

bool IfcGeom::CgalKernel::convert(const IfcSchema::IfcBooleanResult* l, cgal_shape_t& shape) {
  
  cgal_shape_t s1, s2;
  ConversionResults items1, items2;
  cgal_wire_t boundary_wire;
  IfcSchema::IfcBooleanOperand* operand1 = l->FirstOperand();
  IfcSchema::IfcBooleanOperand* operand2 = l->SecondOperand();
  bool is_halfspace = operand2->is(IfcSchema::Type::IfcHalfSpaceSolid);
  
  if ( shape_type(operand1) == ST_SHAPELIST ) {
    Logger::Message(Logger::LOG_ERROR, "s1: ST_SHAPELIST Unsupported", operand1->entity);
//    if (!(convert_shapes(operand1, items1) && flatten_shape_list(items1, s1, true))) {
      return false;
//    }
  } else if ( shape_type(operand1) == ST_SHAPE ) {
    if (!convert_shape(operand1, s1) ) {
      return false;
    }
  } else {
    Logger::Message(Logger::LOG_ERROR, "s1: Invalid representation item for boolean operation", operand1->entity);
    return false;
  }

//  const double first_operand_volume = shape_volume(s1);
//  if ( first_operand_volume <= ALMOST_ZERO )
//    Logger::Message(Logger::LOG_WARNING,"Empty solid for:",l->FirstOperand()->entity);
  
  bool shape2_processed = false;
  if ( shape_type(operand2) == ST_SHAPELIST ) {
    Logger::Message(Logger::LOG_ERROR, "s2: ST_SHAPELIST Unsupported", operand1->entity);
//    shape2_processed = convert_shapes(operand2, items2) && flatten_shape_list(items2, s2, true);
  } else if ( shape_type(operand2) == ST_SHAPE ) {
    shape2_processed = convert_shape(operand2,s2);
  } else {
    Logger::Message(Logger::LOG_ERROR, "s2: Invalid representation item for boolean operation", operand2->entity);
  }

  if (!shape2_processed) {
    shape = s1;
    Logger::Message(Logger::LOG_ERROR,"Failed to convert SecondOperand of:",l->entity);
    return true;
  }

//  if (!is_halfspace) {
//    const double second_operand_volume = shape_volume(s2);
//    if ( second_operand_volume <= ALMOST_ZERO )
//      Logger::Message(Logger::LOG_WARNING,"Empty solid for:",operand2->entity);
//  }
  
  const IfcSchema::IfcBooleanOperator::IfcBooleanOperator op = l->Operator();
  
  if (!s1.is_valid()) {
    Logger::Message(Logger::LOG_ERROR, "s1: Not valid?", operand1->entity);
    return false;
  } else {
//    std::ofstream f1;
//    CGAL::Polyhedron_3<Kernel> p1;
//    s1.convert_to_Polyhedron(p1);
//    f1.open("/Users/ken/Desktop/s1.off");
//    f1 << p1 << std::endl;
//    f1.close();
  }
  
  bool is_plane = false;
  cgal_plane_t plane;
  if (!s2.is_valid()) {
    Logger::Message(Logger::LOG_ERROR, "s2: Not valid?", operand2->entity);
    return false;
  } else if (is_halfspace) {
    //    std::cout << "s2: halfspace" << std::endl;
    IfcSchema::IfcHalfSpaceSolid *hss = static_cast<IfcSchema::IfcHalfSpaceSolid *>(operand2);
    IfcSchema::IfcSurface* surface = hss->BaseSurface();
    if (surface->is(IfcSchema::Type::IfcPlane) ) {
      is_plane = true;
      IfcGeom::CgalKernel::convert((IfcSchema::IfcPlane *)surface, plane);
      if (hss->AgreementFlag()) plane = plane.opposite();
//      std::ofstream fresult;
//      fresult.open("/Users/ken/Desktop/s2.off");
//      fresult << "OFF" << std::endl << "4 2 4" << std::endl;
//      // x = -5, y = -5, z = (5a +5b -d)/c
//      fresult << "-5 -5 " << (5.0*CGAL::to_double(plane.a())+5.0*CGAL::to_double(plane.b())-CGAL::to_double(plane.d()))/CGAL::to_double(plane.c()) << std::endl;
//      // x = -5, y = +5, z = (5a -5b -d)/c
//      fresult << "-5 5 " << (5.0*CGAL::to_double(plane.a())-5.0*CGAL::to_double(plane.b())-CGAL::to_double(plane.d()))/CGAL::to_double(plane.c()) << std::endl;
//      // x = 5, y = -5, z = (-5a +5b -d)/c
//      fresult << "5 -5 " << (-5.0*CGAL::to_double(plane.a())+5.0*CGAL::to_double(plane.b())-CGAL::to_double(plane.d()))/CGAL::to_double(plane.c()) << std::endl;
//      // x = 5, y = +5, z = (-5a -5b -d)/c
//      fresult << "5 5 " << (-5.0*CGAL::to_double(plane.a())-5.0*CGAL::to_double(plane.b())-CGAL::to_double(plane.d()))/CGAL::to_double(plane.c()) << std::endl;
//      fresult << "3 0 1 2" << std::endl;
//      fresult << "3 3 2 1" << std::endl;
//      fresult.close();
    }
  } else {
//    std::ofstream f2;
//    CGAL::Polyhedron_3<Kernel> p2;
//    s2.convert_to_Polyhedron(p2);
//    f2.open("/Users/ken/Desktop/s2.off");
//    f2 << p2 << std::endl;
//    f2.close();
  }
  
  if (op == IfcSchema::IfcBooleanOperator::IfcBooleanOperator_DIFFERENCE) {
    
//    std::cout << "Difference" << std::endl;
    CGAL::Nef_polyhedron_3<Kernel> nef_result(s1);
    if (is_halfspace) {
      if (is_plane) nef_result = nef_result.intersection(plane, CGAL::Nef_polyhedron_3<Kernel>::Intersection_mode::CLOSED_HALFSPACE);
    } else {
      nef_result -= CGAL::Nef_polyhedron_3<Kernel>(s2);
    }
    if (!nef_result.is_simple()) {
      std::cout << "Not simple: " << nef_result.number_of_volumes() << " volumes" << std::endl;
      return false;
    } else {
//      CGAL::Polyhedron_3<Kernel> result;
//      nef_result.convert_to_polyhedron(result);
//      std::ofstream fresult;
//      fresult.open("/Users/ken/Desktop/result.off");
//      fresult << result << std::endl;
//      fresult.close();
    } nef_result.convert_to_polyhedron(shape);
    return true;
    
  } else if (op == IfcSchema::IfcBooleanOperator::IfcBooleanOperator_UNION) {

//    std::cout << "Union" << std::endl;
    CGAL::Nef_polyhedron_3<Kernel> nef_result = CGAL::Nef_polyhedron_3<Kernel>(s1)+CGAL::Nef_polyhedron_3<Kernel>(s2);
    if (!nef_result.is_simple()) {
      std::cout << "Not simple: " << nef_result.number_of_volumes() << " volumes" << std::endl;
      return false;
    } else {
//      CGAL::Polyhedron_3<Kernel> result;
//      nef_result.convert_to_polyhedron(result);
//      std::ofstream fresult;
//      fresult.open("/Users/ken/Desktop/result.off");
//      fresult << result << std::endl;
//      fresult.close();
    } nef_result.convert_to_polyhedron(shape);
    return true;
    
  } else if (op == IfcSchema::IfcBooleanOperator::IfcBooleanOperator_INTERSECTION) {

//    std::cout << "Intersection" << std::endl;
    CGAL::Nef_polyhedron_3<Kernel> nef_result = CGAL::Nef_polyhedron_3<Kernel>(s1)*CGAL::Nef_polyhedron_3<Kernel>(s2);
    if (!nef_result.is_simple()) {
      std::cout << "Not simple: " << nef_result.number_of_volumes() << " volumes" << std::endl;
      return false;
    } else {
//      CGAL::Polyhedron_3<Kernel> result;
//      nef_result.convert_to_polyhedron(result);
//      std::ofstream fresult;
//      fresult.open("/Users/ken/Desktop/result.off");
//      fresult << result << std::endl;
//      fresult.close();
    } nef_result.convert_to_polyhedron(shape);
    return true;
  } return false;
}

bool IfcGeom::CgalKernel::convert(const IfcSchema::IfcSphere* l, cgal_shape_t& shape) {
  const double r = l->Radius() * getValue(GV_LENGTH_UNIT);
  
  // Make icosahedron
  float golden_ratio = (1.0+sqrtf(5.0))/2.0;
  float normalising_factor = sqrtf(golden_ratio*golden_ratio+1.0);
  std::vector<Kernel::Point_3> icosahedron_vertices;
  icosahedron_vertices.push_back(Kernel::Point_3(-1.0/normalising_factor,  golden_ratio/normalising_factor, 0.0));
  icosahedron_vertices.push_back(Kernel::Point_3( 1.0/normalising_factor,  golden_ratio/normalising_factor, 0.0));
  icosahedron_vertices.push_back(Kernel::Point_3(-1.0/normalising_factor, -golden_ratio/normalising_factor, 0.0));
  icosahedron_vertices.push_back(Kernel::Point_3( 1.0/normalising_factor, -golden_ratio/normalising_factor, 0.0));
  icosahedron_vertices.push_back(Kernel::Point_3(0.0, -1.0/normalising_factor,  golden_ratio/normalising_factor));
  icosahedron_vertices.push_back(Kernel::Point_3(0.0,  1.0/normalising_factor,  golden_ratio/normalising_factor));
  icosahedron_vertices.push_back(Kernel::Point_3(0.0, -1.0/normalising_factor, -golden_ratio/normalising_factor));
  icosahedron_vertices.push_back(Kernel::Point_3(0.0,  1.0/normalising_factor, -golden_ratio/normalising_factor));
  icosahedron_vertices.push_back(Kernel::Point_3( golden_ratio/normalising_factor, 0.0, -1.0/normalising_factor));
  icosahedron_vertices.push_back(Kernel::Point_3( golden_ratio/normalising_factor, 0.0,  1.0/normalising_factor));
  icosahedron_vertices.push_back(Kernel::Point_3(-golden_ratio/normalising_factor, 0.0, -1.0/normalising_factor));
  icosahedron_vertices.push_back(Kernel::Point_3(-golden_ratio/normalising_factor, 0.0,  1.0/normalising_factor));
  
  std::list<cgal_face_t> face_list;
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[0]);
  face_list.back().outer.push_back(icosahedron_vertices[11]);
  face_list.back().outer.push_back(icosahedron_vertices[5]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[0]);
  face_list.back().outer.push_back(icosahedron_vertices[5]);
  face_list.back().outer.push_back(icosahedron_vertices[1]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[0]);
  face_list.back().outer.push_back(icosahedron_vertices[1]);
  face_list.back().outer.push_back(icosahedron_vertices[7]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[0]);
  face_list.back().outer.push_back(icosahedron_vertices[7]);
  face_list.back().outer.push_back(icosahedron_vertices[10]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[0]);
  face_list.back().outer.push_back(icosahedron_vertices[10]);
  face_list.back().outer.push_back(icosahedron_vertices[11]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[1]);
  face_list.back().outer.push_back(icosahedron_vertices[5]);
  face_list.back().outer.push_back(icosahedron_vertices[9]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[5]);
  face_list.back().outer.push_back(icosahedron_vertices[11]);
  face_list.back().outer.push_back(icosahedron_vertices[4]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[11]);
  face_list.back().outer.push_back(icosahedron_vertices[10]);
  face_list.back().outer.push_back(icosahedron_vertices[2]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[10]);
  face_list.back().outer.push_back(icosahedron_vertices[7]);
  face_list.back().outer.push_back(icosahedron_vertices[6]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[7]);
  face_list.back().outer.push_back(icosahedron_vertices[1]);
  face_list.back().outer.push_back(icosahedron_vertices[8]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[3]);
  face_list.back().outer.push_back(icosahedron_vertices[9]);
  face_list.back().outer.push_back(icosahedron_vertices[4]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[3]);
  face_list.back().outer.push_back(icosahedron_vertices[4]);
  face_list.back().outer.push_back(icosahedron_vertices[2]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[3]);
  face_list.back().outer.push_back(icosahedron_vertices[2]);
  face_list.back().outer.push_back(icosahedron_vertices[6]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[3]);
  face_list.back().outer.push_back(icosahedron_vertices[6]);
  face_list.back().outer.push_back(icosahedron_vertices[8]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[3]);
  face_list.back().outer.push_back(icosahedron_vertices[8]);
  face_list.back().outer.push_back(icosahedron_vertices[9]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[4]);
  face_list.back().outer.push_back(icosahedron_vertices[9]);
  face_list.back().outer.push_back(icosahedron_vertices[5]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[2]);
  face_list.back().outer.push_back(icosahedron_vertices[4]);
  face_list.back().outer.push_back(icosahedron_vertices[11]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[6]);
  face_list.back().outer.push_back(icosahedron_vertices[2]);
  face_list.back().outer.push_back(icosahedron_vertices[10]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[8]);
  face_list.back().outer.push_back(icosahedron_vertices[6]);
  face_list.back().outer.push_back(icosahedron_vertices[7]);
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(icosahedron_vertices[9]);
  face_list.back().outer.push_back(icosahedron_vertices[8]);
  face_list.back().outer.push_back(icosahedron_vertices[1]);
  
  const unsigned int refinements = 2;
  for (unsigned int current_refinement = 0; current_refinement < refinements; ++current_refinement) {
    std::list<cgal_face_t> refined_face_list;
    for (auto &face: face_list) {
      Kernel::Point_3 vertex0 = face.outer[0];
      Kernel::Point_3 vertex1 = face.outer[1];
      Kernel::Point_3 vertex2 = face.outer[2];
      
      Kernel::Point_3 midpoint01 = CGAL::midpoint(vertex0, vertex1);
      Kernel::Point_3 midpoint12 = CGAL::midpoint(vertex1, vertex2);
      Kernel::Point_3 midpoint20 = CGAL::midpoint(vertex2, vertex0);
      
      double midpoint01_distance_to_origin = sqrt(CGAL::to_double(CGAL::squared_distance(midpoint01, Kernel::Point_3(0, 0, 0))));
      midpoint01 = Kernel::Point_3(midpoint01.x()/midpoint01_distance_to_origin,
                                   midpoint01.y()/midpoint01_distance_to_origin,
                                   midpoint01.z()/midpoint01_distance_to_origin);
      double midpoint12_distance_to_origin = sqrt(CGAL::to_double(CGAL::squared_distance(midpoint12, Kernel::Point_3(0, 0, 0))));
      midpoint12 = Kernel::Point_3(midpoint12.x()/midpoint12_distance_to_origin,
                                   midpoint12.y()/midpoint12_distance_to_origin,
                                   midpoint12.z()/midpoint12_distance_to_origin);
      double midpoint20_distance_to_origin = sqrt(CGAL::to_double(CGAL::squared_distance(midpoint20, Kernel::Point_3(0, 0, 0))));
      midpoint20 = Kernel::Point_3(midpoint20.x()/midpoint20_distance_to_origin,
                                   midpoint20.y()/midpoint20_distance_to_origin,
                                   midpoint20.z()/midpoint20_distance_to_origin);
      
      refined_face_list.push_back(cgal_face_t());
      refined_face_list.back().outer.push_back(vertex0);
      refined_face_list.back().outer.push_back(midpoint01);
      refined_face_list.back().outer.push_back(midpoint20);
      
      refined_face_list.push_back(cgal_face_t());
      refined_face_list.back().outer.push_back(vertex1);
      refined_face_list.back().outer.push_back(midpoint12);
      refined_face_list.back().outer.push_back(midpoint01);
      
      refined_face_list.push_back(cgal_face_t());
      refined_face_list.back().outer.push_back(vertex2);
      refined_face_list.back().outer.push_back(midpoint20);
      refined_face_list.back().outer.push_back(midpoint12);
      
      refined_face_list.push_back(cgal_face_t());
      refined_face_list.back().outer.push_back(midpoint01);
      refined_face_list.back().outer.push_back(midpoint12);
      refined_face_list.back().outer.push_back(midpoint20);
    } face_list = refined_face_list;
  }
  
  cgal_placement_t trsf;
  IfcGeom::CgalKernel::convert(l->Position(),trsf);
  
  shape = create_polyhedron(face_list);
  for (auto &vertex: vertices(shape)) {
    vertex->point() = Kernel::Point_3(r*vertex->point().x(),
                                      r*vertex->point().y(),
                                      r*vertex->point().z());
    vertex->point() = vertex->point().transform(trsf);
  }
  return true;
}

bool IfcGeom::CgalKernel::convert(const IfcSchema::IfcRectangularPyramid* l, cgal_shape_t& shape) {
  const double dx = l->XLength() * getValue(GV_LENGTH_UNIT);
  const double dy = l->YLength() * getValue(GV_LENGTH_UNIT);
  const double dz = l->Height() * getValue(GV_LENGTH_UNIT);
  
  std::list<cgal_face_t> face_list;
  
  // Base
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(Kernel::Point_3(0, 0, 0));
  face_list.back().outer.push_back(Kernel::Point_3(dx, 0, 0));
  face_list.back().outer.push_back(Kernel::Point_3(dx, dy, 0));
  face_list.back().outer.push_back(Kernel::Point_3(0, dy, 0));
  
  // Lateral faces
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(Kernel::Point_3(0, 0, 0));
  face_list.back().outer.push_back(Kernel::Point_3(0, dy, 0));
  face_list.back().outer.push_back(Kernel::Point_3(0.5*dx, 0.5*dy, dz));
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(Kernel::Point_3(0, dy, 0));
  face_list.back().outer.push_back(Kernel::Point_3(dx, dy, 0));
  face_list.back().outer.push_back(Kernel::Point_3(0.5*dx, 0.5*dy, dz));
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(Kernel::Point_3(dx, dy, 0));
  face_list.back().outer.push_back(Kernel::Point_3(dx, 0, 0));
  face_list.back().outer.push_back(Kernel::Point_3(0.5*dx, 0.5*dy, dz));
  
  face_list.push_back(cgal_face_t());
  face_list.back().outer.push_back(Kernel::Point_3(dx, 0, 0));
  face_list.back().outer.push_back(Kernel::Point_3(0, 0, 0));
  face_list.back().outer.push_back(Kernel::Point_3(0.5*dx, 0.5*dy, dz));
  
  cgal_placement_t trsf;
  IfcGeom::CgalKernel::convert(l->Position(),trsf);
  
  shape = create_polyhedron(face_list);
  for (auto &vertex: vertices(shape)) vertex->point() = vertex->point().transform(trsf);
  return true;
}

bool IfcGeom::CgalKernel::convert(const IfcSchema::IfcRightCircularCylinder* l, cgal_shape_t& shape) {
  const double r = l->Radius() * getValue(GV_LENGTH_UNIT);
  const double h = l->Height() * getValue(GV_LENGTH_UNIT);
  
  std::list<cgal_face_t> face_list;
  
  const int segments = 12;
  
  // Base
  face_list.push_back(cgal_face_t());
  for (int current_segment = 0; current_segment < segments; ++current_segment) {
    double current_angle = current_segment*2.0*3.141592653589793/((double)segments);
    face_list.back().outer.push_back(Kernel::Point_3(r*cos(current_angle), r*sin(current_angle), 0));
  }
  
  // Side faces
  for (int current_segment = 0; current_segment < segments; ++current_segment) {
    double current_angle = current_segment*2.0*3.141592653589793/((double)segments);
    int next_segment = (current_segment+1)%segments;
    double next_angle = next_segment*2.0*3.141592653589793/((double)segments);
    face_list.push_back(cgal_face_t());
    face_list.back().outer.push_back(Kernel::Point_3(r*cos(next_angle), r*sin(next_angle), 0));
    face_list.back().outer.push_back(Kernel::Point_3(r*cos(current_angle), r*sin(current_angle), 0));
    face_list.back().outer.push_back(Kernel::Point_3(r*cos(current_angle), r*sin(current_angle), h));
    face_list.back().outer.push_back(Kernel::Point_3(r*cos(next_angle), r*sin(next_angle), h));
  }
  
  // Top
  face_list.push_back(cgal_face_t());
  for (int current_segment = segments-1; current_segment >= 0; --current_segment) {
    double current_angle = current_segment*2.0*3.141592653589793/((double)segments);
    face_list.back().outer.push_back(Kernel::Point_3(r*cos(current_angle), r*sin(current_angle), h));
  }
  
  cgal_placement_t trsf;
  IfcGeom::CgalKernel::convert(l->Position(),trsf);
  
  shape = create_polyhedron(face_list);
  for (auto &vertex: vertices(shape)) vertex->point() = vertex->point().transform(trsf);
  return true;
}

bool IfcGeom::CgalKernel::convert(const IfcSchema::IfcRightCircularCone* l, cgal_shape_t& shape) {
  const double r = l->BottomRadius() * getValue(GV_LENGTH_UNIT);
  const double h = l->Height() * getValue(GV_LENGTH_UNIT);
  
  std::list<cgal_face_t> face_list;
  
  const int segments = 12;
  
  // Base
  face_list.push_back(cgal_face_t());
  for (int current_segment = 0; current_segment < segments; ++current_segment) {
    double current_angle = current_segment*2.0*3.141592653589793/((double)segments);
    face_list.back().outer.push_back(Kernel::Point_3(r*cos(current_angle), r*sin(current_angle), 0));
  }
  
  // Side faces
  for (int current_segment = 0; current_segment < segments; ++current_segment) {
    double current_angle = current_segment*2.0*3.141592653589793/((double)segments);
    int next_segment = (current_segment+1)%segments;
    double next_angle = next_segment*2.0*3.141592653589793/((double)segments);
    face_list.push_back(cgal_face_t());
    face_list.back().outer.push_back(Kernel::Point_3(r*cos(next_angle), r*sin(next_angle), 0));
    face_list.back().outer.push_back(Kernel::Point_3(r*cos(current_angle), r*sin(current_angle), 0));
    face_list.back().outer.push_back(Kernel::Point_3(0, 0, h));
  }
  
  cgal_placement_t trsf;
  IfcGeom::CgalKernel::convert(l->Position(),trsf);
  
  shape = create_polyhedron(face_list);
  for (auto &vertex: vertices(shape)) vertex->point() = vertex->point().transform(trsf);
  return true;
}

#ifdef USE_IFC4
bool IfcGeom::CgalKernel::convert(const IfcSchema::IfcTriangulatedFaceSet* l, cgal_shape_t& shape) {
  IfcSchema::IfcCartesianPointList3D* point_list = l->Coordinates();
  const std::vector< std::vector<double> > coordinates = point_list->CoordList();
  std::vector<cgal_point_t> points;
  points.reserve(coordinates.size());
  for (std::vector< std::vector<double> >::const_iterator it = coordinates.begin(); it != coordinates.end(); ++it) {
    const std::vector<double>& coords = *it;
    if (coords.size() != 3) {
      Logger::Message(Logger::LOG_ERROR, "Invalid dimensions encountered on Coordinates", l->entity);
      return false;
    }
    points.push_back(Kernel::Point_3(coords[0] * getValue(GV_LENGTH_UNIT),
                                     coords[1] * getValue(GV_LENGTH_UNIT),
                                     coords[2] * getValue(GV_LENGTH_UNIT)));
  }
  
  std::vector< std::vector<int> > indices = l->CoordIndex();
  
  std::list<cgal_face_t> face_list;
  
  for(std::vector< std::vector<int> >::const_iterator it = indices.begin(); it != indices.end(); ++ it) {
    const std::vector<int>& tri = *it;
    if (tri.size() != 3) {
      Logger::Message(Logger::LOG_ERROR, "Invalid dimensions encountered on CoordIndex", l->entity);
      return false;
    }
    
    const int min_index = *std::min_element(tri.begin(), tri.end());
    const int max_index = *std::max_element(tri.begin(), tri.end());
    
    if (min_index < 1 || max_index > (int) points.size()) {
      Logger::Message(Logger::LOG_ERROR, "Contents of CoordIndex out of bounds", l->entity);
      return false;
    }
    
    const Kernel::Point_3& a = points[tri[0] - 1]; // account for zero- vs
    const Kernel::Point_3& b = points[tri[1] - 1]; // one-based indices in
    const Kernel::Point_3& c = points[tri[2] - 1]; // c++ and express
    
    face_list.push_back(cgal_face_t());
    face_list.back().outer.push_back(a);
    face_list.back().outer.push_back(b);
    face_list.back().outer.push_back(c);
  }
  
  shape = create_polyhedron(face_list);
  return true;
}
#endif

bool IfcGeom::CgalKernel::convert(const IfcSchema::IfcHalfSpaceSolid* l, cgal_shape_t& shape) {
  IfcSchema::IfcSurface* surface = l->BaseSurface();
  if ( ! surface->is(IfcSchema::Type::IfcPlane) ) {
    Logger::Message(Logger::LOG_ERROR, "Unsupported BaseSurface:", surface->entity);
    return false;
  }
  cgal_plane_t pln;
  IfcGeom::CgalKernel::convert((IfcSchema::IfcPlane*)surface,pln);
  
  // TODO: Don't fully understand the logic here. Might be incorrect.
  if (l->AgreementFlag()) pln = pln.opposite();
//  const gp_Pnt pnt = pln.Location().Translated( l->AgreementFlag() ? -pln.Axis().Direction() : pln.Axis().Direction());
//  shape = BRepPrimAPI_MakeHalfSpace(BRepBuilderAPI_MakeFace(pln),pnt).Solid();
  
  // TODO: For now we do nothing and process halfspaces in IfcBooleanResult, which likely doesn't capture all cases.
  // Find a better solution later (with an abstract shape class?)
  shape = CGAL::Polyhedron_3<Kernel>();
  return true;
}
