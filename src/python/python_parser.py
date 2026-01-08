import ast
import hashlib


class PythonParser:
    """
    Parser for Python statements that generate new symbols.
    
    Analyzes Python code and extracts information about variables, functions, classes,
    and imports. Only supports statements that create new symbols in the namespace.
    """
    
    def __init__(self):
        # Map of supported AST node types to their category names
        self.valid_types = {
            "FunctionDef": "func",
            "AsyncFunctionDef": "func",
            "ClassDef": "class",
            "Import": "import",
            "ImportFrom": "import_from",
            "Assign": "var",
            "AnnAssign": "var",
        }

    @staticmethod
    def _collect_function_param_names(args_node):
        """Collect all parameter names from a function arguments node.
        
        Args:
            args_node: AST arguments node
            
        Returns:
            Set of parameter names
        """
        param_names = set()
        for arg in args_node.args:
            param_names.add(arg.arg)
        if args_node.vararg:
            param_names.add(args_node.vararg.arg)
        if args_node.kwarg:
            param_names.add(args_node.kwarg.arg)
        for arg in args_node.kwonlyargs:
            param_names.add(arg.arg)
        return param_names

    @staticmethod
    def _collect_body_local_names(body):
        """Collect locally defined names from a function or class body.
        
        Args:
            body: List of AST statement nodes
            
        Returns:
            Set of locally defined names
        """
        local_names = set()
        for stmt in body:
            if isinstance(stmt, (ast.FunctionDef, ast.AsyncFunctionDef)):
                local_names.add(stmt.name)
            elif isinstance(stmt, ast.ClassDef):
                local_names.add(stmt.name)
            elif isinstance(stmt, ast.Assign):
                for target in stmt.targets:
                    if isinstance(target, ast.Name):
                        local_names.add(target.id)
            elif isinstance(stmt, ast.AnnAssign):
                if isinstance(stmt.target, ast.Name):
                    local_names.add(stmt.target.id)
        return local_names

    def split_statements(self, code):
        """Split multi-statement code into individual statements.
        
        Args:
            code: Python source code containing one or more statements
            
        Returns:
            List of individual statement strings
        """
        tree = ast.parse(code)
        statements = []

        for stmt in tree.body:
            stmt_code = ast.get_source_segment(code, stmt)
            if stmt_code:
                statements.append(stmt_code.strip())
            else:
                statements.append(code.strip())

        return statements

    def parse_single_statement(self, code):
        """Parse a single Python statement and extract symbol information.
        
        Args:
            code: Python source code containing exactly one statement
            
        Returns:
            List of dicts with keys: name, content, type, dependencies, message, status
            
        Raises:
            ValueError: If code doesn't contain exactly one statement
            SyntaxError: If code has syntax errors
        """
        tree = ast.parse(code)

        if len(tree.body) != 1:
            raise ValueError(
                f"parse_single_statement() expects exactly one statement, found {len(tree.body)}"
            )

        statement = tree.body[0]
        result = self._parse_statement_node(statement, code)

        if isinstance(result, list):
            return result
        return [result]

    def parse_multiple_statements(self, code):
        """Parse multiple Python statements and extract all symbol information.
        
        Args:
            code: Python source code containing one or more statements
            
        Returns:
            List of dicts, one for each generated symbol
        """
        statements = self.split_statements(code)

        results = []
        for stmt_code in statements:
            stmt_results = self.parse_single_statement(stmt_code)
            results.extend(stmt_results)

        return results

    def parse_expression_dependencies(self, expression_str):
        """Extract all variable dependencies from a Python expression.
        
        Args:
            expression_str: A Python expression (not a statement)
            
        Returns:
            List of variable names and attribute paths used in the expression
        """
        expr_node = ast.parse(expression_str, mode="eval").body
        dependencies = self._extract_dependencies(expr_node)
        return dependencies

    def _parse_statement_node(self, statement, code):
        """Dispatch statement to appropriate analyzer based on its type.
        
        Args:
            statement: AST node representing the statement
            code: Original source code string
            
        Returns:
            Dict or list of dicts with parsed symbol information
            
        Raises:
            ValueError: If statement type doesn't generate new symbols
            NotImplementedError: If analyzer for statement type is missing
        """
        statement_type = type(statement).__name__

        if statement_type not in self.valid_types:
            raise ValueError(
                f"Unsupported statement type '{statement_type}': only statements that generate new symbols are supported"
            )

        analyzer_method = getattr(self, f"_analyze_{statement_type}", None)
        if analyzer_method:
            return analyzer_method(statement, code)
        raise NotImplementedError(
            f"Analyzer not implemented for statement type: {statement_type}"
        )

    def compute_code_hash(self, code):
        """Compute a hash of the AST structure for caching purposes.
        
        Uses normalized AST representation (ast.dump) to generate consistent
        hashes for the same code, regardless of formatting differences.
        
        Args:
            code: Python source code
            
        Returns:
            Hex string hash of the AST structure
        """
        tree = ast.parse(code)
        ast_str = ast.dump(tree)
        return hashlib.md5(ast_str.encode()).hexdigest()

    def _analyze_FunctionDef(self, node, code):
        """Analyze a function definition statement.
        
        Extracts external variables used in function body, decorators, and default
        parameters as dependencies. Filters out function parameters and local 
        variables to find only truly external variable references.
        
        Args:
            node: AST FunctionDef node
            code: Original source code
            
        Returns:
            Dict with function name and metadata
        """
        # Collect parameter names and local variables defined in function
        local_names = self._collect_function_param_names(node.args)
        local_names.update(self._collect_body_local_names(node.body))
        
        # Extract dependencies from decorators and default arguments
        all_refs = []
        for decorator in node.decorator_list:
            all_refs.extend(self._extract_dependencies(decorator))
        
        for default in node.args.defaults:
            all_refs.extend(self._extract_dependencies(default))
        for default in (node.args.kw_defaults or []):
            if default is not None:
                all_refs.extend(self._extract_dependencies(default))
        
        # Extract dependencies from function body while skipping local names
        for stmt in node.body:
            all_refs.extend(self._extract_dependencies(stmt, skip_names=local_names))
        
        # Remove duplicates while preserving order
        dependencies = list(dict.fromkeys(all_refs))
        
        return {
            "name": node.name,
            "dependencies": dependencies,
            "type": "Function",
            "content": code.strip(),
            "message": "",
            "status": "Success"
        }

    def _analyze_AsyncFunctionDef(self, node, code):
        """Analyze an async function definition statement.
        
        Delegates to _analyze_FunctionDef as async functions have the same
        dependency analysis requirements as regular functions.
        
        Args:
            node: AST AsyncFunctionDef node
            code: Original source code
            
        Returns:
            Dict with function name and metadata
        """
        return self._analyze_FunctionDef(node, code)

    def _analyze_ClassDef(self, node, code):
        """Analyze a class definition statement.
        
        Extracts external variables used in class body, base classes, decorators,
        and metaclass arguments as dependencies. Filters out class attributes 
        and method parameters to find only truly external variable references.
        
        Args:
            node: AST ClassDef node
            code: Original source code
            
        Returns:
            Dict with class name and metadata
        """
        # Collect all variables defined within the class
        class_internal_names = self._collect_body_local_names(node.body)
        
        # Extract dependencies from decorators, base classes, and keywords
        all_refs = []
        for decorator in node.decorator_list:
            all_refs.extend(self._extract_dependencies(decorator))
        
        for base in node.bases:
            all_refs.extend(self._extract_dependencies(base))
        
        for keyword in node.keywords:
            all_refs.extend(self._extract_dependencies(keyword.value))
        
        # Extract dependencies from class body while skipping class internals
        for stmt in node.body:
            all_refs.extend(self._extract_dependencies(stmt, skip_names=class_internal_names))
        
        # Remove duplicates while preserving order
        dependencies = list(dict.fromkeys(all_refs))
        
        return {
            "name": node.name,
            "dependencies": dependencies,
            "type": "Class",
            "content": code.strip(),
            "message": "",
            "status": "Success"
        }

    def _analyze_Import(self, node, code):
        """Analyze an import statement.
        
        Handles both simple imports (import x) and aliased imports (import x as y).
        Multiple imports in one statement are split into separate results.
        
        Args:
            node: AST Import node
            code: Original source code
            
        Returns:
            List of dicts, one for each imported module
        """
        results = []

        for alias in node.names:
            name = alias.asname if alias.asname else alias.name

            single_import_code = f"import {alias.name}"
            if alias.asname:
                single_import_code += f" as {alias.asname}"

            results.append(
                {
                    "name": name,
                    "dependencies": [],
                    "type": "Import",
                    "content": single_import_code,
                    "message": "",
                    "status": "Success"
                }
            )

        return results

    def _analyze_ImportFrom(self, node, code):
        """Analyze a from...import statement.
        
        Handles specific imports (from x import y) and aliased imports (from x import y as z).
        Wildcard imports (from x import *) are not supported as they don't generate 
        deterministic symbols.
        
        Args:
            node: AST ImportFrom node
            code: Original source code
            
        Returns:
            List of dicts, one for each imported name
            
        Raises:
            ValueError: If statement uses wildcard import (import *)
        """
        # Reject wildcard imports - cannot determine specific symbols
        if any(alias.name == "*" for alias in node.names):
            raise ValueError("Cannot determine specific symbols from 'import *' statement")

        results = []
        for alias in node.names:
            name = alias.asname if alias.asname else alias.name

            single_import_code = f"from {node.module} import {alias.name}"
            if alias.asname:
                single_import_code += f" as {alias.asname}"

            results.append(
                {
                    "name": name,
                    "dependencies": [],
                    "type": "ImportFrom",
                    "content": single_import_code,
                    "message": "",
                    "status": "Success"
                }
            )

        return results

    def _analyze_Assign(self, node, code):
        """Analyze a simple assignment statement.
        
        Only supports single-target assignments (x = value).
        Multi-target assignments (x = y = 1) and unpacking (x, y = 1, 2) are not supported.
        
        Args:
            node: AST Assign node
            code: Original source code
            
        Returns:
            Dict with variable name, dependencies, and metadata
            
        Raises:
            ValueError: If assignment has multiple targets or non-name target
        """
        if len(node.targets) != 1:
            raise ValueError("Assignment statement can only have one target variable")

        target = node.targets[0]
        if not isinstance(target, ast.Name):
            raise ValueError("Assignment target must be a variable name")

        dependencies = self._extract_dependencies(node.value)
        value_code = ast.get_source_segment(code, node.value)

        return {
            "name": target.id,
            "dependencies": dependencies,
            "type": "Variable",
            "content": value_code.strip() if value_code else code.strip(),
            "message": "",
            "status": "Success"
        }

    def _analyze_AnnAssign(self, node, code):
        """Analyze a type-annotated assignment statement.
        
        Handles both annotated assignments with values (x: int = 5) and 
        annotation-only declarations (x: int).
        
        Args:
            node: AST AnnAssign node
            code: Original source code
            
        Returns:
            Dict with variable name, dependencies, and metadata
            
        Raises:
            ValueError: If target is not a simple variable name
        """
        if not isinstance(node.target, ast.Name):
            raise ValueError("Annotated assignment target must be a variable name")
        
        # Extract dependencies from value if present, otherwise empty
        dependencies = []
        if node.value is not None:
            dependencies = self._extract_dependencies(node.value)
            value_code = ast.get_source_segment(code, node.value)
            content = value_code.strip() if value_code else code.strip()
        else:
            # Type annotation only, no assignment
            content = code.strip()

        return {
            "name": node.target.id,
            "dependencies": dependencies,
            "type": "Variable",
            "content": content,
            "message": "",
            "status": "Success"
        }

    class _DependencyVisitor(ast.NodeVisitor):
        """AST visitor that collects variable references with nested scope handling."""
        
        def __init__(self, deps_list, skip_set):
            self.deps_list = deps_list
            # Maintain a stack of skip sets to respect nested scopes
            self._skip_stack = [set(skip_set) if skip_set else set()]

        def _current_skip(self):
            return self._skip_stack[-1]

        def visit_Name(self, node):
            """Record simple variable names that are being read."""
            if isinstance(node.ctx, ast.Load):
                if node.id not in self._current_skip():
                    self.deps_list.append(node.id)

        def visit_Attribute(self, node):
            """Record attribute access chains (e.g., x.y.z)."""
            if isinstance(node.ctx, ast.Load):
                if isinstance(node.value, (ast.Name, ast.Attribute)):
                    attr_path = self._get_base_attribute_path(node)
                    if attr_path:
                        # Add all prefixes: x, x.y, x.y.z
                        parts = attr_path.split(".")
                        for i in range(1, len(parts) + 1):
                            partial_path = ".".join(parts[:i])
                            base = parts[0]
                            if base not in self._current_skip():
                                self.deps_list.append(partial_path)
            self.generic_visit(node)

        def visit_NamedExpr(self, node):
            """Handle walrus operator: dependencies come from RHS only."""
            # Evaluate RHS dependencies
            self.visit(node.value)
            # Treat the walrus target as a local binding in the current scope
            if isinstance(node.target, ast.Name):
                self._current_skip().add(node.target.id)
            elif isinstance(node.target, (ast.Tuple, ast.List)):
                for name in self._collect_target_names(node.target):
                    self._current_skip().add(name)

        def _collect_target_names(self, target):
            """Collect all names from assignment targets (handles unpacking)."""
            names = set()
            if isinstance(target, ast.Name):
                names.add(target.id)
            elif isinstance(target, (ast.Tuple, ast.List)):
                for elt in target.elts:
                    names.update(self._collect_target_names(elt))
            return names

        def _visit_comprehension_like(self, generators, extra_skip=None, body_visit=None):
            """Handle comprehension scopes: targets introduce local names."""
            local_names = set(extra_skip) if extra_skip else set()
            for gen in generators:
                local_names.update(self._collect_target_names(gen.target))
            self._skip_stack.append(self._current_skip().union(local_names))
            # Visit generator iter expressions and ifs
            for gen in generators:
                self.visit(gen.iter)
                for if_clause in gen.ifs:
                    self.visit(if_clause)
            if body_visit:
                body_visit()
            self._skip_stack.pop()

        def visit_ListComp(self, node):
            self._visit_comprehension_like(node.generators, body_visit=lambda: self.visit(node.elt))

        def visit_SetComp(self, node):
            self._visit_comprehension_like(node.generators, body_visit=lambda: self.visit(node.elt))

        def visit_GeneratorExp(self, node):
            self._visit_comprehension_like(node.generators, body_visit=lambda: self.visit(node.elt))

        def visit_DictComp(self, node):
            self._visit_comprehension_like(
                node.generators,
                body_visit=lambda: (self.visit(node.key), self.visit(node.value))
            )

        def visit_FunctionDef(self, node):
            """Handle nested function scope: skip parameters and locals."""
            local_names = PythonParser._collect_function_param_names(node.args)
            local_names.update(PythonParser._collect_body_local_names(node.body))
            
            # Visit decorators and default arguments BEFORE entering function scope
            # (they are evaluated in the outer scope)
            for decorator in node.decorator_list:
                self.visit(decorator)
            for default in node.args.defaults:
                self.visit(default)
            for default in (node.args.kw_defaults or []):
                if default is not None:
                    self.visit(default)
            
            # Enter scope
            self._skip_stack.append(self._current_skip().union(local_names))
            for stmt in node.body:
                self.visit(stmt)
            self._skip_stack.pop()

        def visit_AsyncFunctionDef(self, node):
            """Handle nested async function similar to FunctionDef."""
            return self.visit_FunctionDef(node)

        def visit_ClassDef(self, node):
            """Handle nested class scope: skip class internals."""
            class_locals = PythonParser._collect_body_local_names(node.body)
            
            # Visit decorators and base classes BEFORE entering class scope
            # (they are evaluated in the outer scope)
            for decorator in node.decorator_list:
                self.visit(decorator)
            for base in node.bases:
                self.visit(base)
            for keyword in node.keywords:
                self.visit(keyword.value)
            
            # Enter class scope
            self._skip_stack.append(self._current_skip().union(class_locals))
            for stmt in node.body:
                self.visit(stmt)
            self._skip_stack.pop()

        def visit_Lambda(self, node):
            """Handle lambda scope: skip parameters while visiting defaults and body."""
            local_names = PythonParser._collect_function_param_names(node.args)

            # Visit defaults BEFORE entering scope
            for default in node.args.defaults:
                self.visit(default)
            for default in (node.args.kw_defaults or []):
                if default is not None:
                    self.visit(default)
            
            # Enter scope and visit body
            self._skip_stack.append(self._current_skip().union(local_names))
            self.visit(node.body)
            self._skip_stack.pop()

        def _get_base_attribute_path(self, node):
            """Recursively build attribute path, stopping at function calls."""
            if isinstance(node, ast.Attribute):
                left_part = self._get_base_attribute_path(node.value)
                if left_part and not self._contains_call(node.value):
                    return f"{left_part}.{node.attr}"
                return None
            if isinstance(node, ast.Name):
                return node.id
            return None

        def _contains_call(self, node):
            """Check if node or its chain contains a function call."""
            if isinstance(node, ast.Call):
                return True
            if isinstance(node, ast.Attribute):
                return self._contains_call(node.value)
            if isinstance(node, ast.Name):
                return False
            return False

    def _extract_dependencies(self, node, skip_names=None):
        """Extract all variable dependencies from an AST node.
        
        Extracts both simple names (x) and attribute chains (x.y.z).
        For attribute chains, includes all prefixes (x, x.y, x.y.z).
        Ignores attributes accessed on function call results.
        
        Args:
            node: AST node to analyze
            skip_names: Optional set of base names to skip (locals/params)
            
        Returns:
            List of dependency names (deduplicated, order preserved)
        """
        dependencies = []
        skip_set = set(skip_names) if skip_names else set()

        visitor = self._DependencyVisitor(dependencies, skip_set)
        visitor.visit(node)

        return list(dict.fromkeys(dependencies))