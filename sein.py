from __future__ import annotations

"""
sein.py — SEIN config parser (Python 3.8+)

Async:
    result = parse_sein(path, usage_async=True)
    result.wait()              # block until parsing is done #
    cfg = result.data          # then use the config #
"""

import os
import textwrap
import threading
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Optional

Config = dict[str, dict[str, str]]


# Internal helpers #

def _trim(s: str) -> str:
    return s.strip()


def _strip_comment(line: str) -> str:
    """Strip # comments, respecting quoted strings"""
    in_quote = False
    for i, c in enumerate(line):
        if c == '"':
            in_quote = not in_quote
        if not in_quote and c == '#':
            return line[:i]
    return line


def _strip_quotes(val: str) -> str:
    if len(val) >= 2 and val[0] == '"' and val[-1] == '"':
        return val[1:-1]
    return val


def _dedent(s: str) -> str:
    return textwrap.dedent(s)


def _substitute_value(
    val: str,
    config: Config,
    vars_: dict[str, str],
) -> str:
    """Expand ${VAR}, ${section.key}, ${SYSENV:VAR} references"""
    if not val:
        return val

    result: list[str] = []
    pos = 0
    while pos < len(val):
        start = val.find('$', pos)
        if start == -1:
            result.append(val[pos:])
            break

        if start + 1 < len(val) and val[start + 1] == '{':
            end = val.find('}', start + 2)
            if end != -1:
                result.append(val[pos:start])
                var_name = val[start + 2:end]

                # ${SYSENV:VAR} - always OS environment, never @set #
                if var_name.startswith('SYSENV:'):
                    result.append(os.environ.get(var_name[7:], ''))
                else:
                    dot = var_name.find('.')
                    if dot != -1:
                        section = var_name[:dot]
                        key = var_name[dot + 1:]
                        result.append(config.get(section, {}).get(key, ''))
                    else:
                        # @set takes priority over OS environment #
                        if var_name in vars_:
                            result.append(vars_[var_name])
                        else:
                            result.append(os.environ.get(var_name, ''))

                pos = end + 1
                continue

        result.append(val[pos:start + 1])
        pos = start + 1

    return ''.join(result)


def _parse_file(
    path: str,
    result: Config,
    vars_: dict[str, str],
    inherit: dict[str, str],   # child -> parent #
    depth: int = 0,
) -> None:
    if depth > 8:
        print(f"[SEIN Parser]: @include depth limit reached at: {path}")
        return

    try:
        with open(path, 'r', encoding='utf-8') as f:
            lines = f.read().splitlines()
    except OSError:
        print(f"[SEIN Parser]: Failed to open: {path}")
        return

    base_dir = os.path.dirname(path)
    if base_dir:
        base_dir += os.sep

    current_section = "Default"

    multiline_key: str = ''
    multiline_val: str = ''
    in_multiline: bool = False

    raw_key: str = ''
    raw_val: str = ''
    in_raw: bool = False

    for line in lines:
        ## raw-string mode ##
        if in_raw:
            end_pos = line.find(')R"')
            if end_pos != -1:
                raw_val += line[:end_pos]
                final_val = _substitute_value(_dedent(raw_val), result, vars_)
                result.setdefault(current_section, {})[raw_key] = final_val
                raw_key = raw_val = ''
                in_raw = False
            else:
                raw_val += line + '\n'
            continue

        ## multiline mode ##
        if in_multiline:
            clean = _trim(_strip_comment(line))
            continues = bool(clean) and clean[-1] == '\\'
            if continues:
                clean = clean[:-1]
            stripped = _trim(clean)
            multiline_val += ' ' + stripped
            if not continues:
                final_val = _substitute_value(
                    _strip_quotes(_trim(multiline_val)), result, vars_
                )
                result.setdefault(current_section, {})[multiline_key] = final_val
                multiline_key = multiline_val = ''
                in_multiline = False
            continue

        clean = _trim(_strip_comment(line))
        if not clean:
            continue

        ## @set ##
        if clean.startswith('@set') and (len(clean) == 4 or clean[4] in (' ', '\t')):
            rest = _trim(clean[4:])
            eq = rest.find('=')
            if eq != -1:
                var_name = _trim(rest[:eq])
                var_val  = _strip_quotes(_trim(rest[eq + 1:]))
                vars_[var_name] = _substitute_value(var_val, result, vars_)
            continue

        ## @include ##
        if clean.startswith('@include'):
            inc = _strip_quotes(_trim(clean[8:]))
            inc_path = os.path.join(base_dir, inc) if base_dir else inc
            _parse_file(inc_path, result, vars_, inherit, depth + 1)
            continue

        ## [Section] or [Child : [Parent]] ##
        if clean.startswith('[') and clean.endswith(']'):
            inner = _trim(clean[1:-1])
            colon = inner.find(':')
            if colon != -1:
                child_name  = _trim(inner[:colon])
                parent_raw  = _trim(inner[colon + 1:])
                # parent_raw should look like "[Parent]"
                if parent_raw.startswith('['):
                    parent_raw = parent_raw[1:]
                    if parent_raw.endswith(']'):
                        parent_raw = parent_raw[:-1]
                parent_name = _trim(parent_raw)
                current_section = child_name
                if parent_name:
                    inherit[current_section] = parent_name
            else:
                current_section = inner
            result.setdefault(current_section, {})
            continue

        ## key = value ##
        sep = clean.find('=')
        if sep == -1:
            continue

        key = _trim(clean[:sep])
        val = _trim(clean[sep + 1:])

        ## Raw string "R( ... )R" ##
        if val.startswith('"R('):
            end_pos = val.find(')R"', 3)
            if end_pos != -1:
                raw_str = _substitute_value(val[3:end_pos], result, vars_)
                result.setdefault(current_section, {})[key] = raw_str
            else:
                raw_key = key
                raw_val = val[3:] + '\n'
                in_raw  = True
            continue

        ## Multiline continuation ##
        if val and val[-1] == '\\':
            multiline_key = key
            multiline_val = _trim(val[:-1])
            in_multiline  = True
            continue

        final_val = _substitute_value(_strip_quotes(val), result, vars_)
        result.setdefault(current_section, {})[key] = final_val


def _resolve_inheritance(result: Config, inherit: dict[str, str]) -> None:
    """Copy parent keys into child sections (child keys take priority)."""
    for child, parent in inherit.items():
        parent_sec = result.get(parent)
        if not parent_sec:
            continue
        child_sec = result.setdefault(child, {})
        for k, v in parent_sec.items():
            if k not in child_sec:
                child_sec[k] = v


# SeinResult - returned by parse_sein #

class SeinResult:
    """Holds parsed config data plus an optional async thread handle"""

    def __init__(self, data: Config, thread: Optional[threading.Thread] = None):
        self.data   = data
        self._thread = thread

    def wait(self) -> None:
        """Block until async parsing is complete (no-op for sync)"""
        if self._thread and self._thread.is_alive():
            self._thread.join()
        self._thread = None

    def done(self) -> bool:
        """True if parsing is complete."""
        return self._thread is None or not self._thread.is_alive()

    # Convenience pass-through so callers can use result[section] directly #
    def __getitem__(self, key: str) -> dict[str, str]:
        return self.data[key]

    def get(self, key: str, default=None):
        return self.data.get(key, default)


# Public parser API #

def parse_sein(path: str, usage_async: bool = False) -> SeinResult:
    """Parse a .sein config file

    If usage_async=True the call returns immediately; call result.wait()
    before accessing result.data
    """
    if not usage_async:
        result: Config = {}
        vars_:  dict[str, str] = {}
        inherit: dict[str, str] = {}
        _parse_file(path, result, vars_, inherit)
        _resolve_inheritance(result, inherit)
        return SeinResult(result)

    # Async: parse in a background thread #
    result: Config = {}
    vars_:  dict[str, str] = {}
    inherit: dict[str, str] = {}

    def _worker() -> None:
        _parse_file(path, result, vars_, inherit)
        _resolve_inheritance(result, inherit)

    t = threading.Thread(target=_worker, daemon=True)
    sr = SeinResult(result, t)
    t.start()
    return sr


# Getter helpers #

def get_value(cfg: Config, section: str, key: str, fallback: str = '') -> str:
    return cfg.get(section, {}).get(key, fallback)


def get_int(cfg: Config, section: str, key: str, fallback: int = 0) -> int:
    val = get_value(cfg, section, key)
    if not val:
        return fallback
    try:
        return int(val)
    except (ValueError, TypeError):
        return fallback


def get_float(cfg: Config, section: str, key: str, fallback: float = 0.0) -> float:
    val = get_value(cfg, section, key)
    if not val:
        return fallback
    try:
        return float(val)
    except (ValueError, TypeError):
        return fallback


def get_bool(cfg: Config, section: str, key: str, fallback: bool = False) -> bool:
    val = get_value(cfg, section, key).lower()
    if not val:
        return fallback
    if val in ('true', 'yes', '1'):
        return True
    if val in ('false', 'no', '0'):
        return False
    return fallback


def split_value(val: str, delim: str = ';') -> list[str]:
    return [_trim(t) for t in val.split(delim) if _trim(t)]


def get_array(cfg: Config, section: str, key: str, delim: str = ';') -> list[str]:
    return split_value(get_value(cfg, section, key), delim)


def get_int_array(cfg: Config, section: str, key: str, delim: str = ';') -> list[int]:
    result = []
    for t in get_array(cfg, section, key, delim):
        try:
            result.append(int(t))
        except (ValueError, TypeError):
            result.append(0)
    return result


def get_float_array(cfg: Config, section: str, key: str, delim: str = ';') -> list[float]:
    result = []
    for t in get_array(cfg, section, key, delim):
        try:
            result.append(float(t))
        except (ValueError, TypeError):
            result.append(0.0)
    return result


# Writer API - data classes #

@dataclass
class SeinKeyValue:
    key:     str
    value:   str
    comment: str = ''


@dataclass
class SeinSection:
    name:    str
    comment: str = ''
    entries: list[SeinKeyValue] = field(default_factory=list)


class DirectiveKind(Enum):
    Include = auto()
    Set     = auto()
    Comment = auto()
    Blank   = auto()


@dataclass
class SeinDirective:
    kind:    DirectiveKind = DirectiveKind.Blank
    operand: str = ''
    value:   str = ''
    comment: str = ''


@dataclass
class SeinDocument:
    path:         str = ''
    directives:   list[SeinDirective] = field(default_factory=list)
    sections:     list[SeinSection]   = field(default_factory=list)
    usage_async:  bool = False


# Document construction helpers #

def create_new_config(path: str, usage_async: bool = False) -> SeinDocument:
    """Create a new empty SeinDocument.

    usage_async=True stores the flag (reserved for future async saves).
    """
    return SeinDocument(path=path, usage_async=usage_async)


def add_include(doc: SeinDocument, path: str, comment: str = '') -> None:
    doc.directives.append(
        SeinDirective(DirectiveKind.Include, operand=path, comment=comment)
    )


def add_global_var(doc: SeinDocument, name: str, value: str, comment: str = '') -> None:
    for d in doc.directives:
        if d.kind == DirectiveKind.Set and d.operand == name:
            d.value   = value
            d.comment = comment
            return
    doc.directives.append(
        SeinDirective(DirectiveKind.Set, operand=name, value=value, comment=comment)
    )


def add_header_comment(doc: SeinDocument, text: str) -> None:
    doc.directives.append(SeinDirective(DirectiveKind.Comment, operand=text))


def add_blank_line(doc: SeinDocument) -> None:
    doc.directives.append(SeinDirective(DirectiveKind.Blank))


def find_section(doc: SeinDocument, name: str) -> Optional[SeinSection]:
    for s in doc.sections:
        if s.name == name:
            return s
    return None


def add_section(doc: SeinDocument, name: str, comment: str = '') -> SeinSection:
    existing = find_section(doc, name)
    if existing:
        return existing
    sec = SeinSection(name=name, comment=comment)
    doc.sections.append(sec)
    return sec


def remove_section(doc: SeinDocument, name: str) -> None:
    doc.sections = [s for s in doc.sections if s.name != name]


def add_value(doc: SeinDocument, section: str, key: str,
              value: str, comment: str = '') -> None:
    sec = add_section(doc, section)
    for kv in sec.entries:
        if kv.key == key:
            kv.value   = value
            kv.comment = comment
            return
    sec.entries.append(SeinKeyValue(key=key, value=value, comment=comment))


def remove_value(doc: SeinDocument, section: str, key: str) -> None:
    sec = find_section(doc, section)
    if sec:
        sec.entries = [kv for kv in sec.entries if kv.key != key]


# Serialization helpers #

def _needs_quotes(v: str) -> bool:
    if not v:
        return True
    if v[0] in (' ', '\t') or v[-1] in (' ', '\t'):
        return True
    return any(c in v for c in ('#', '=', ' ', '\t'))


def _quote_if_needed(v: str) -> str:
    return f'"{v}"' if _needs_quotes(v) else v


def _inline_comment(comment: str) -> str:
    return f'  # {comment}' if comment else ''


# save_config #

def save_config(doc: SeinDocument, path: str = '') -> bool:
    target = path or doc.path
    try:
        with open(target, 'w', encoding='utf-8') as f:
            for d in doc.directives:
                if d.kind == DirectiveKind.Blank:
                    f.write('\n')
                elif d.kind == DirectiveKind.Comment:
                    f.write(f'# {d.operand}\n')
                elif d.kind == DirectiveKind.Include:
                    f.write(f'@include "{d.operand}"{_inline_comment(d.comment)}\n')
                elif d.kind == DirectiveKind.Set:
                    f.write(
                        f'@set {d.operand} = {_quote_if_needed(d.value)}'
                        f'{_inline_comment(d.comment)}\n'
                    )

            if doc.directives and doc.sections:
                f.write('\n')

            for i, sec in enumerate(doc.sections):
                if sec.comment:
                    f.write(f'# {sec.comment}\n')
                f.write(f'[{sec.name}]\n')
                for kv in sec.entries:
                    f.write(
                        f'{kv.key} = {_quote_if_needed(kv.value)}'
                        f'{_inline_comment(kv.comment)}\n'
                    )
                if i + 1 < len(doc.sections):
                    f.write('\n')
        return True
    except OSError as e:
        print(f'[SEIN Writer]: Failed to open for writing: {target} — {e}')
        return False


# load_as_document #

def load_as_document(path: str) -> SeinDocument:
    """Parse an existing .sein file into a SeinDocument (round-trip safe)."""
    doc = SeinDocument(path=path)

    try:
        with open(path, 'r', encoding='utf-8') as f:
            lines = f.read().splitlines()
    except OSError as e:
        print(f'[SEIN Writer]: Failed to open: {path} — {e}')
        return doc

    current_section: str = ''
    in_header: bool = True

    for line in lines:
        line = line.rstrip('\r')
        sv   = _trim(line)

        if not sv:
            if in_header:
                add_blank_line(doc)
            continue

        if sv.startswith('#'):
            if in_header:
                add_header_comment(doc, _trim(sv[1:]))
            continue

        if sv.startswith('@include'):
            inc = _strip_quotes(_trim(sv[8:]))
            add_include(doc, inc)
            continue

        if sv.startswith('@set'):
            rest = _trim(sv[4:])
            eq = rest.find('=')
            if eq != -1:
                name = _trim(rest[:eq])
                val  = _strip_quotes(_trim(rest[eq + 1:]))
                add_global_var(doc, name, val)
            continue

        if sv.startswith('[') and sv.endswith(']'):
            in_header = False
            current_section = _trim(sv[1:-1])
            add_section(doc, current_section)
            continue

        eq = sv.find('=')
        if eq != -1 and current_section:
            key = _trim(sv[:eq])
            val = _trim(sv[eq + 1:])
            hash_pos = val.find('#')
            if hash_pos != -1:
                val = _trim(val[:hash_pos])
            val = _strip_quotes(val)
            add_value(doc, current_section, key, val)

    return doc