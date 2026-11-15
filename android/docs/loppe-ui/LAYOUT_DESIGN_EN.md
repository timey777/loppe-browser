# LOPPE Deterministic Layout Design

LOPPE layout is designed for predictable application UI. It intentionally does not reproduce CSS intrinsic minimum sizing or the full CSS Flexbox algorithm.

> 📖 [中文版](LAYOUT_DESIGN.md) | English

---

## Size Model

Every node has three relevant sizes:

1. Preferred size: explicit `width`/`height`, or measured content size for `auto`.
2. Final size: the preferred size after min/max constraints and flex allocation.
3. Scroll size: the actual content extent when content exceeds the final size.

Content can increase the preferred or scroll size. It must not enlarge a final size that has already been assigned by flex layout.

## Defaults

| Property | Default | Meaning |
|---|---:|---|
| `min-width`, `min-height` | `0` | No implicit content minimum |
| `max-width`, `max-height` | unbounded | No maximum constraint |
| `flex-grow` | `0` | Does not consume remaining space |
| `flex-shrink` | `0` | Does not participate in space reduction |

`width` and `height` are preferred sizes, not minimum sizes. An item can become smaller than its preferred size only when it explicitly sets `flex-shrink`.

Unlike standard CSS, a flex item has **no implicit `min-width: auto`**. Since the size minimum already defaults to `0`, the `min-width: 0` habit carried over from CSS (to let a flex item shrink below its content width) is unnecessary here.

## Box Model

LOPPE uses one sizing model: `width`, `height`, `min-*`, and `max-*` describe the border box. Padding and border are included in those dimensions. Margin is outside the declared size.

For example, `width: 100px; padding: 10px; border: 2px` produces a 100px border box and a 76px content box.

## Min And Max Constraints

Min/max constraints are explicit hard bounds:

```text
base size = clamp(preferred size, minimum size, maximum size)
```

There is no implicit `min-content` or `min-height:auto` behavior. If an explicit minimum is greater than an explicit maximum, the minimum wins.

Percentages resolve against the parent's content box only when that parent axis has a definite size. Otherwise the percentage behaves as `auto`.

## Flex Allocation

Grow and shrink are independent and operate only on the main axis.

When free space is positive, eligible items receive it in direct proportion to their `flex-grow` values. An item freezes at its maximum, and remaining space is redistributed among the other eligible items.

When free space is negative, only items with `flex-shrink > 0` participate. The deficit is divided directly by shrink value, not by `size * shrink`. An item freezes at its minimum, and the remaining deficit is redistributed.

If no eligible item remains, unresolved space becomes container overflow. Items with the default `flex-shrink: 0` are never reduced implicitly.

In a single flex line, `align-items: stretch` may always grow an auto cross size. It may reduce the item to the container cross size only when the item's `overflow` is `auto`, `scroll`, or `hidden`; that item then handles excess content. With `overflow: visible`, the content size is preserved and its overflow is propagated to the parent.

## Grid Allocation

Grid track sizing follows the same deterministic principles as flex:

1. **Fixed tracks first**: `px`, `%`, and `auto` tracks (content-sized) claim space first; an `auto` track takes the largest content size among the items on that track.
2. **Remaining-space allocation**: after subtracting fixed tracks and gaps from the container content size, the remaining space is distributed to `fr` tracks proportionally to their `fr` values; when the remainder is negative, `fr` tracks become 0 and the overflow is handled by the container's overflow.
3. **Unknown container size**: `fr` and `%` tracks cannot resolve (treated as 0); the container's intrinsic size is decided by `px`/`auto` tracks — consistent with flex auto width being content-driven.
4. **Stretch**: items with `width`/`height` set to auto stretch to fill their cell (minus their own margin/border/padding), matching flex cross-axis stretch semantics.
5. **Block-axis alignment (align-items / align-self)**: default `stretch` is item 4 above; with `center` / `flex-end` / `flex-start` items keep their content height and align within the row (offset = row height − item height, half of it for center, clamped at 0). A child's `align-self` overrides the parent `align-items` (`auto` = follow parent).

## Scroll Containers

A flexible scroll region must opt into shrinking:

```loppe
@layout {
    .app { display: flex; flex-direction: column; }
    .content { flex-grow: 1; flex-shrink: 1; }
}
```

The content region receives the remaining final height. Excess descendants increase its `scrollHeight`; they do not enlarge the assigned final height.

This keeps fixed toolbars and status bars stable while making the intended content region responsible for overflow.

### Per-axis scrolling (`overflow-x` / `overflow-y`, 2026-10-23)

The single `overflow` keyword controls both axes. To scroll on one axis only, use the per-axis properties:

```loppe
@style {
    /* Scroll vertically; do not clip horizontally, no horizontal bar —
       horizontal overflow is handed to the outer scroll container */
    .list { overflow-y: auto; overflow-x: visible; }
}
```

- `overflow-x` / `overflow-y` override their own axis (`visible` = no clipping, no bar on that axis, overflow goes to the nearest scrolling ancestor; `hidden` = clip without a bar; `auto`/`scroll` = scroll itself).
- `overflow` remains the shorthand (sets both axes) and remains the input for flex cross-axis stretch (`visible` preserves the content size).
- Common combo: **scroll vertically only, don't show horizontal overflow** → `overflow-y: auto; overflow-x: hidden` (clipped in the box, zero horizontal scroll range).
- ⚠ Overflow on a `visible` axis is *painted*: an inner scroll container's clip rect travels with its content, so an ancestor's horizontal scroll does **not** reveal what the inner container clipped away. If you only want "no horizontal bar", use `hidden` — otherwise the content is drawn past its own vertical scrollbar.
